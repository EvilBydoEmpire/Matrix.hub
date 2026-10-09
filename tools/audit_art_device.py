"""Capture color-only upgrade baseline and verify retained device state.

Never saves presets, and restores the live selection/auto-cycle. Wi-Fi names
and passwords aren't written to the report, only a comparison hash.
"""
import argparse,json,time,hashlib,urllib.request
from pathlib import Path
from profile_pattern_library import MatrixHttp
p=argparse.ArgumentParser();p.add_argument('mode',choices=['before','after']);p.add_argument('--host',required=True);args=p.parse_args()
m=MatrixHttp(args.host);state=m.json_get('CONTROL');library=m.json_get('LIBRARY')
rawGet=m.json_get;rawCommand=m.command
def retry(fn,*a):
 for attempt in range(3):
  try:
   time.sleep(.12);return fn(*a)
  except (OSError,ValueError):
   if attempt==2:raise
   time.sleep(1)
m.json_get=lambda *a:retry(rawGet,*a)
m.command=lambda *a:retry(rawCommand,*a)
restore={k:state[k] for k in ('currentPattern','params','targetFPS','autoCycleEnabled')}
Path('/private/tmp/art-live-restore.json').write_text(json.dumps(restore))
snapshot=Path('/private/tmp/art-device-before.json')
colorKeys={'hue','spread','palette','globalHueShift','globalSaturation','color0','color1','color2','color3'}
def networkHash(s):return hashlib.sha256(json.dumps([{k:n.get(k) for k in ('index','ssid','preferred')} for n in s['savedNetworks']],sort_keys=True).encode()).hexdigest()
archived={r[0] for r in library['entries'] if len(r)>5 and r[5]}
rows=[]
try:
 m.command('setAutoCycle',False)
 m.command('setFPS',30)
 for item in state['catalog']:
  a=item['engine'];v=item['variant'];id=a if v is None else 16+v
  if id in archived or (v is not None and v>=65):continue # Never unarchive or start temporal studies for an audit.
  try:
   m.command('setPattern' if v is None else 'setPatternVariant',a if v is None else v)
   s=m.json_get('CONTROL');rows.append({'id':id,'name':item['name'],'params':s['params']});print(item['name'],flush=True)
  finally:
   pass
finally:
 a=state['currentPattern'];v=state['params'].get('variant',0)
 if (a if a!=16 else 16+v) not in archived:
  m.command('setPatternVariant' if a==16 else 'setPattern',v if a==16 else a)
  for k,val in state['params'].items():
   if k!='variant' and (args.mode=='before' or k not in colorKeys):m.command('setParam',{'animIndex':a,'paramName':k,'value':val})
 m.command('setFPS',state['targetFPS']);m.command('setAutoCycle',state['autoCycleEnabled'])
result={'patterns':rows,'library':library,'bookmarks':state['bookmarks'],'networks':networkHash(state)}
assert m.json_get('LIBRARY')==library
if args.mode=='before':snapshot.write_text(json.dumps(result));print('Baseline captured: '+str(len(rows))+' patterns, no Wi-Fi credentials recorded')
else:
 old=json.loads(snapshot.read_text());assert result['networks']==old['networks'];assert result['library']==old['library'];assert result['bookmarks'][:len(old['bookmarks'])]==old['bookmarks']
 for row,prev in zip(rows,old['patterns']):
  assert row['id']==prev['id']
  assert {k:v for k,v in row['params'].items() if k not in colorKeys}=={k:v for k,v in prev['params'].items() if k not in colorKeys},row['name']
  assert row['params']['palette']==7 and row['params']['globalSaturation']==100,row['name']
 Path('/private/tmp/art-device-after.json').write_text(json.dumps(result))
 print(f'PASS authored defaults; {len(rows)} active motion presets, bookmarks, metadata and networks preserved; archived and strobe studies not started')
