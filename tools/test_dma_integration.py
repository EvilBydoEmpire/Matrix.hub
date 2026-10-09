"""Exercise the full DMA profile without saving temporary test settings."""
import json
import time
from pathlib import Path
from profile_pattern_library import MatrixHttp

m=MatrixHttp('192.168.178.115')
before=m.json_get('CONTROL')
assert before['backend']=='hub75-dma-exact20-test'
cal={k:before['calibration'][k] for k in ('r','g','b','gamma100','photoCorrection')}
rows=[]
def sample(label):
    time.sleep(2)
    d=m.json_get('DIAGNOSTICS')
    assert d['colorDepth']==4 and d['displayClockMHz']==20 and d['refreshMeasured']
    assert d['panelRefreshHz']>700 and d['freeHeap']>30000
    rows.append({'test':label,**{k:d[k] for k in ('pattern','engine','panelRefreshHz','actualFPS','freeHeap')}})
    print(json.dumps(rows[-1]),flush=True)

try:
    m.command('setPattern',0);sample('native-Starfield')
    m.command('setPatternVariant',57);sample('photo')
    m.command('setColorCalibration',dict(cal,r=90,g=95));sample('channel-balance')
    m.command('setColorCalibration',cal)
    m.command('runLuaHello',None);sample('Lua')
    m.command('stopLuaHello',None)
    m.command('setCalibrationMode',1);sample('calibration-chart')
finally:
    m.command('stopLuaHello',None)
    m.command('setColorCalibration',cal)
    m.command('setCalibrationMode',0)
    m.command('setPatternVariant' if before['currentPattern']==16 else 'setPattern',
              before['params']['variant'] if before['currentPattern']==16 else before['currentPattern'])
    for key,value in before['params'].items():
        if key!='variant':m.command('setParam',dict(animIndex=before['currentPattern'],paramName=key,value=value))
    if before['calibration']['mode']:m.command('setCalibrationMode',before['calibration']['mode'])
    after=m.json_get('CONTROL')
    for key in ('params','bookmarks','targetFPS','autoCycleEnabled'):
        assert after[key]==before[key],key
    assert len(after['catalog'])==len(before['catalog'])==55
    Path('docs/dma-full-integration.json').write_text(json.dumps(rows,indent=2)+'\n')
    print('Restored playback; catalog, bookmarks, parameters and playback settings preserved',flush=True)
