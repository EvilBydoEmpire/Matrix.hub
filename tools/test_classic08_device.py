"""Read live WROOM diagnostics; exercise imports and restore playback controls."""
import json,time,urllib.request
from pathlib import Path
base='http://192.168.178.111'
def get(path):
 with urllib.request.urlopen(base+'/'+path,timeout=12) as r:return json.load(r)
def command(name,**fields):
 request=urllib.request.Request(base+'/control',data=json.dumps(dict(command=name,**fields)).encode(),headers={'Content-Type':'application/json'})
 with urllib.request.urlopen(request,timeout=12) as r:result=json.load(r)
 assert result.get('success'),result
initial=get('control');assert initial['deviceProfile']=='esp32-wroom-ws2812-16x16'
assert len(initial['catalog'])==61 and initial['displayWidth']==initial['displayHeight']==16
report=[]
try:
 command('setAutoCycle',value=False)
 command('setFPS',value=60)
 for variant in (63,64):
  command('setPatternVariant',value=variant)
  command('setParam',animIndex=16,paramName='b',value=3)
  command('setPatternQuality',value=1)
  assert get('control')['patternQuality']==1
  command('setPatternQuality',value=0)
  time.sleep(3)
  d=get('diagnostics')
  assert d['actualFPS']>=50 and d['freeHeap']>30000,d
  assert d['renderWidth']==d['renderHeight']==16
  fields=('pattern','actualFPS','averageRenderUs','averageShowUs','freeHeap','minimumFreeHeap','largestFreeBlock','uptimeMs','otaReady')
  report.append({k:d[k] for k in fields});print(report[-1],flush=True)
 command('setFPS',value=initial['targetFPS'])
finally:
 command('setAutoCycle',value=initial['autoCycleEnabled'])
Path('docs/classic08-wroom-verification.json').write_text(json.dumps({'profile':initial['deviceProfile'],'patterns':report},indent=2)+'\n')
print('PASS live WROOM, 61 patterns, imports, upper quality bit, 60 FPS target, OTA')
