"""Record real on-device Soukan defaults; retain RGB colors, no RGB332/dither.

Run with the workspace Pillow Python. The editable source atlas remains the
filter-zero RGB PNG used by other tools; the firmware embeds an adaptive-indexed
copy of the same images to fit WROOM OTA partitions. Does not save any presets.
"""
import json,time,urllib.request,struct
from pathlib import Path
from PIL import Image
from profile_pattern_library import MatrixHttp,png_bytes,ASSET

root=Path(__file__).resolve().parent.parent
m=MatrixHttp('192.168.178.115');initial=m.json_get('CONTROL');library=m.json_get('LIBRARY')
frames=24;interval=.067;width=height=28
atlas=Image.new('RGB',(width*frames,height*len(initial['catalog'])))
archived={r[0] for r in library['entries'] if len(r)>5 and r[5]}
rows=[];review=Path('/private/tmp/soukan-review');review.mkdir(exist_ok=True)
try:
 m.command('setAutoCycle',False)
 for index,item in enumerate(initial['catalog']):
  engine=item['engine'];variant=item['variant'];id=engine if variant is None else 16+variant
  try:
   if id in archived:m.command('setPatternArchived',{'id':id,'archived':False})
   m.command('setPattern' if variant is None else 'setPatternVariant',engine if variant is None else variant)
   params=m.json_get('CONTROL')['params'];assert 11<=params['palette']<=20
   time.sleep(1.5);timestamps=[];requestTimes=[]
   for frame in range(frames):
    start=time.monotonic()
    with urllib.request.urlopen(m.base+'/preview?format=rgb888-binary',timeout=15) as response:raw=response.read()
    assert len(raw)==width*height*3,len(raw)
    requestTimes.append(time.monotonic()-start);timestamps.append(time.monotonic())
    gain=15/max(1,params['b'])
    # One scalar per pixel, preserving hue even if an additive source exceeds
    # the user's nominal brightness. No per-channel gamma or RGB332 palette.
    out=bytearray(len(raw))
    for p in range(0,len(raw),3):
     light=max(raw[p:p+3]);scale=min(gain,255/light) if light else gain
     for c in range(3):out[p+c]=round(raw[p+c]*scale)
    atlas.paste(Image.frombytes('RGB',(width,height),bytes(out)),(frame*width,index*height))
    time.sleep(max(0,interval-(time.monotonic()-start)))
   # Refresh the native before/after contact sheet with the final color code.
   for frame in (0,3):
    with urllib.request.urlopen(m.base+'/preview?format=rgb565-native',timeout=15) as response:raw=response.read()
    assert len(raw)==8192
    rgb=bytes(v for c in struct.unpack('<4096H',raw) for v in (((c>>11)&31)*255//31,((c>>5)&63)*255//63,(c&31)*255//31))
    (review/f'{index:02d}-{frame}.rgb').write_bytes(rgb)
    time.sleep(.35)
   row={'index':index,'name':item['name'],'params':params,'captureIntervalMs':round((timestamps[-1]-timestamps[0])*1000/(frames-1),2),'requestMeanMs':round(sum(requestTimes)/frames*1000,2)}
   rows.append(row);print(f"{index+1}/59 {item['name']} — {row['captureIntervalMs']} ms/frame",flush=True)
  finally:
   if id in archived:m.command('setPatternArchived',{'id':id,'archived':True})
finally:
 engine=initial['currentPattern'];variant=initial['params'].get('variant',0);id=engine if engine!=16 else 16+variant
 if id not in archived:
  m.command('setPatternVariant' if engine==16 else 'setPattern',variant if engine==16 else engine)
  for k,v in initial['params'].items():
   if k!='variant':m.command('setParam',{'animIndex':engine,'paramName':k,'value':v})
 m.command('setAutoCycle',initial['autoCycleEnabled'])
assert m.json_get('LIBRARY')['entries']==library['entries']
assert m.json_get('CONTROL')['bookmarks']==initial['bookmarks']
before=json.loads(Path('/private/tmp/matrix-soukan-before.json').read_text())
colorKeys={'palette','hue','spread','globalHueShift','globalSaturation','color0','color1','color2','color3'}
for row,old in zip(rows,before['patterns']):
 assert row['name']==old['name']
 assert {k:v for k,v in row['params'].items() if k not in colorKeys}=={k:v for k,v in old['params'].items() if k not in colorKeys},row['name']
(root/'docs/soukan-review.json').write_text(json.dumps({'patterns':rows,'preserved':['motion parameters','brightness','style','LFO','bookmarks','ratings/tags/archive']},indent=2)+'\n')
# Keep a lossless source usable by read_generated_atlas and all older tools.
ASSET.write_bytes(png_bytes(atlas.width,atlas.height,atlas.tobytes()))
report={'frames':frames,'previewIntervalMs':round(interval*1000),'patterns':rows}
(root/'docs/soukan-preview-capture.json').write_text(json.dumps(report,indent=2)+'\n')
from pack_soukan_preview import pack
pack()
print(f'PASS {len(rows)} real previews',flush=True)
