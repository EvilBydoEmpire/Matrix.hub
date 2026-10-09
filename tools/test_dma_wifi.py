"""Bounded isolated-probe experiment. Never use against production firmware.

Serial controls work when Wi-Fi fails. Every run ends by blanking/stopping DMA.
Reports contain no network credentials or photo data.
"""
import argparse
import json
import time
import urllib.request
from pathlib import Path
import serial

p=argparse.ArgumentParser()
p.add_argument('--port',default='/dev/cu.usbmodem2101')
p.add_argument('--host',default='192.168.178.115')
p.add_argument('--report',type=Path,required=True)
p.add_argument('--mode',choices=['isolate','confirm'],default='isolate')
a=p.parse_args()
rows=[]
with serial.Serial(a.port,115200,timeout=.1,write_timeout=2) as link:
    def command(cmd):
        link.reset_input_buffer()
        link.write((cmd+'\n').encode());link.flush()
        end=time.monotonic()+4
        while time.monotonic()<end:
            line=link.readline().decode(errors='replace').strip()
            if line.startswith('{'):
                state=json.loads(line)
                if not str(state.get('probe','')).startswith('matrixportal-dma-'):
                    raise RuntimeError('Not the isolated probe')
                return state
        raise TimeoutError('USB response missing: '+cmd)

    def phase(name,commands=(),samples=5):
        for cmd in commands:command(cmd)
        time.sleep(1.1)
        before=command('STATS'); results=[]
        for _ in range(samples):
            start=time.monotonic()
            try:
                with urllib.request.urlopen('http://'+a.host+'/diagnostics',timeout=1.5) as r:
                    state=json.load(r)
                if state['running']!=before['running']:
                    raise RuntimeError('Display state changed during phase; discard sample')
                results.append({'ms':round((time.monotonic()-start)*1000,2),
                                'running':state['running'],'scanHz':state['addressEScanHz']})
            except Exception as e:results.append({'error':type(e).__name__})
            time.sleep(.15)
        row={'phase':name,'before':before,'after':command('STATS'),'http':results}
        rows.append(row)
        print(json.dumps({'phase':name,'success':sum('ms' in r for r in results),
                          'requests':samples,'stats':row['after']}),flush=True)
        a.report.write_text(json.dumps(rows,indent=2)+'\n')

    initial=command('STATS')
    if initial['running']:raise RuntimeError('Reboot probe before test')
    try:
        phase('display-off',samples=5)
        if a.mode=='isolate':
            phase('exact-clock-drive3',['START20'])
            phase('drive1',['DRIVE1'])
            phase('drive0',['DRIVE0'])
            phase('no-render-drive0',['DRAW0'])
            phase('blank-drive0',['BLANK'])
            phase('no-sleep-drive0',['UNBLANK','DRAW1','NOSLEEP'])
            phase('no-sleep-drive2',['DRIVE2'])
            phase('no-sleep-drive3',['DRIVE3'])
        else:
            phase('candidate-moving',['DRIVE3','NOSLEEP','PLASMA','START20'],samples=200)
            phase('candidate-photo',['PHOTO1'],samples=80)
    finally:
        print('STOP',json.dumps(command('STOP')),flush=True)
        phase('display-stopped',samples=5)
