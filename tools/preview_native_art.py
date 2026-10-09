"""Capture the ten native renderers using proposed colors, without saving presets."""
import argparse,json,re,time,urllib.request
from pathlib import Path
from PIL import Image
from profile_pattern_library import MatrixHttp,ASSET,png_bytes
from pack_soukan_preview import pack
ap=argparse.ArgumentParser();ap.add_argument('--host',required=True);args=ap.parse_args()
root=Path(__file__).resolve().parent.parent
colors=[re.findall(r'0x([0-9A-F]{6})',l) for l in (root/'src/patterns/ArtDirection.h').read_text().splitlines() if ' {0x' in l]
m=MatrixHttp(args.host);state=m.json_get('CONTROL');library=m.json_get('LIBRARY')
Path('/private/tmp/native-art-restore.json').write_text(json.dumps({k:state[k] for k in ('params','currentPattern','autoCycleEnabled','targetFPS')}))
archived={r[0] for r in library['entries'] if len(r)>5 and r[5]}
atlas=Image.open(ASSET).convert('RGB')
try:
 m.command('setAutoCycle',False);m.command('setFPS',60)
 for row,item in enumerate(state['catalog'][:10]):
  a=item['engine']
  try:
   if a in archived:m.command('setPatternArchived',{'id':a,'archived':False})
   m.command('setPattern',a);params=m.json_get('CONTROL')['params']
   edits={'palette':7,'hue':0,'spread':360,'globalHueShift':0,'globalSaturation':100,**{f'color{i}':'#'+c for i,c in enumerate(colors[a])}}
   for k,v in edits.items():m.command('setParam',{'animIndex':a,'paramName':k,'value':v})
   time.sleep(.5)
   for f in range(24):
    start=time.monotonic()
    with urllib.request.urlopen(m.base+'/preview?format=rgb888-binary',timeout=15) as r:raw=r.read()
    assert len(raw)==28*28*3
    gain=15/max(1,params['b']);data=bytearray(raw)
    for i in range(0,len(raw),3):
     peak=max(raw[i:i+3]);scale=min(gain,255/peak) if peak else gain
     for c in range(3):data[i+c]=round(raw[i+c]*scale)
    atlas.paste(Image.frombytes('RGB',(28,28),bytes(data)),(f*28,row*28))
    time.sleep(max(0,.067-(time.monotonic()-start)))
   print(item['name'],flush=True)
  finally:
   if a in archived:m.command('setPatternArchived',{'id':a,'archived':True})
finally:
 a=state['currentPattern'];v=state['params'].get('variant',0)
 if (a if a!=16 else 16+v) not in archived:
  m.command('setPatternVariant' if a==16 else 'setPattern',v if a==16 else a)
  for k,val in state['params'].items():
   if k!='variant':m.command('setParam',{'animIndex':a,'paramName':k,'value':val})
 m.command('setFPS',state['targetFPS']);m.command('setAutoCycle',state['autoCycleEnabled'])
ASSET.write_bytes(png_bytes(atlas.width,atlas.height,atlas.tobytes()));pack()
finalLibrary=m.json_get('LIBRARY')
for key in ('entries','folders','families','tags','readable'):assert finalLibrary[key]==library[key]
print('PASS native previews; no presets saved')
