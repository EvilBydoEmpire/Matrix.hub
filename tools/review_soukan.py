"""Snapshot/verify the color-only migration and capture all default patterns.
Only the explicitly selected default-color layer changes. Never saves presets.
Temporary archive/playback changes are restored even after an error.
"""
import argparse,json,time,urllib.request,struct
from pathlib import Path
from profile_pattern_library import MatrixHttp
root=Path(__file__).resolve().parent.parent
p=argparse.ArgumentParser();p.add_argument('mode',choices=['before','after']);p.add_argument('--host',default='192.168.178.115');p.add_argument('--only',type=int,action='append');p.add_argument('--states-only',action='store_true');a=p.parse_args()
m=MatrixHttp(a.host);snapshot=Path('/private/tmp/matrix-soukan-before.json');review=Path('/private/tmp/soukan-review');review.mkdir(exist_ok=True)
state=m.json_get('CONTROL');library=m.json_get('LIBRARY')
keep={k:state[k] for k in ['currentPattern','params','autoCycleEnabled','targetFPS','bookmarks','catalog','defaultPatches']}
colorKeys={'palette','hue','spread','globalHueShift','globalSaturation','color0','color1','color2','color3'}
archived={r[0] for r in library['entries'] if len(r)>5 and r[5]}
before=json.loads(snapshot.read_text()) if a.mode=='after' else {'state':keep,'library':library,'patterns':[]}
if a.mode=='before':snapshot.write_text(json.dumps(before,indent=2))
rows=[]
try:
 m.command('setAutoCycle',False)
 for index,item in enumerate(state['catalog']):
  if a.only and index not in a.only:continue
  engine=item['engine'];v=item['variant'];id=engine if v is None else 16+v
  try:
   if id in archived:m.command('setPatternArchived',{'id':id,'archived':False})
   m.command('setPattern' if v is None else 'setPatternVariant',engine if v is None else v)
   current=m.json_get('CONTROL');params=current['params']
   row={'index':index,'name':item['name'],'id':id,'params':params}
   if a.mode=='before':
    before['patterns'].append(row);snapshot.write_text(json.dumps(before,indent=2))
   else:
    previous=before['patterns'][index]['params']
    assert {k:z for k,z in params.items() if k not in colorKeys}=={k:z for k,z in previous.items() if k not in colorKeys},('Non-color preset changed',item['name'])
    assert 11<=params['palette']<=20,(item,params)
    if not a.states_only:time.sleep(.8)
    for frame in range(0 if a.states_only else 4):
     with urllib.request.urlopen(m.base+'/preview?format=rgb565-native',timeout=10) as response:raw=response.read()
     assert len(raw)==8192
     rgb=bytes(v for c in struct.unpack('<4096H',raw) for v in (((c>>11)&31)*255//31,((c>>5)&63)*255//63,(c&31)*255//31))
     (review/f'{index:02d}-{frame}.rgb').write_bytes(rgb)
     time.sleep(.22)
    if not a.states_only:
     diag=m.json_get('DIAGNOSTICS');row['fps']=diag['actualFPS'];row['drawUs']=diag['averageDrawUs']
   rows.append(row);print(f"{index+1}/59 {item['name']} — palette {params['palette']}",flush=True)
  finally:
   if id in archived:m.command('setPatternArchived',{'id':id,'archived':True})
finally:
 s=keep
 id=s['currentPattern'] if s['currentPattern']!=16 else 16+s['params']['variant']
 if id not in archived:
  m.command('setPatternVariant' if s['currentPattern']==16 else 'setPattern',s['params']['variant'] if s['currentPattern']==16 else s['currentPattern'])
  for key,value in s['params'].items():
   if key!='variant' and (a.mode=='before' or key not in colorKeys):m.command('setParam',{'animIndex':s['currentPattern'],'paramName':key,'value':value})
 m.command('setAutoCycle',s['autoCycleEnabled'])
if a.mode=='after':
 after=m.json_get('CONTROL');lib=m.json_get('LIBRARY')
 assert after['bookmarks']==before['state']['bookmarks']
 assert lib['entries']==before['library']['entries'],'metadata or archive changed'
 (root/'docs/soukan-review.json').write_text(json.dumps({'patterns':rows,'preserved':['motion parameters','brightness','style','LFO','bookmarks','ratings/tags/archive']},indent=2)+'\n')
print('PASS snapshot / migration review',flush=True)
