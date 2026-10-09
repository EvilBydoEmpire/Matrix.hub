"""Data visualization of exact hardware snapshots, not generated artwork."""
from pathlib import Path
import json,re
from PIL import Image,ImageDraw
root=Path(__file__).resolve().parent.parent
data=json.loads(Path('/private/tmp/matrix-soukan-before.json').read_text())['patterns']
folder=Path('/private/tmp/soukan-review')
text=(root/'src/patterns/SoukanPalettes.h').read_text()
colors=[re.findall('0x([0-9A-F]{6})',r) for r in text.split('constexpr uint32_t Colors')[1].split('};')[0].splitlines() if '0x' in r]
pal=Image.new('RGB',(760,10*66),'#171c20');draw=ImageDraw.Draw(pal)
for i,row in enumerate(colors):
 draw.text((12,i*66+10),f'Soukan {i+1}',fill='white')
 for j,c in enumerate(row):
  draw.rectangle((126+j*153,i*66+5,273+j*153,i*66+44),fill='#'+c)
  draw.text((128+j*153,i*66+47),'#'+c,fill='#d3d5d7')
pal.save('/private/tmp/soukan-palettes.png')
for page in range(4):
 sheet=Image.new('RGB',(744,5*181),'#15191e');draw=ImageDraw.Draw(sheet)
 for j,row in enumerate(data[page*15:(page+1)*15]):
  x=(j%3)*248;y=(j//3)*181;index=row['index']
  name=row['name'].replace('Warped Checkerboard — ','WC / ').replace('Bioluminescent Topo Vortex','Topo Vortex')
  draw.text((x+5,y+3),f'{index:02d} {name[:34]}',fill='white')
  for n,frame in enumerate([0,3]):
   f=folder/f'{index:02d}-{frame}.rgb'
   if not f.exists():continue
   image=Image.frombytes('RGB',(64,64),f.read_bytes())
   # Normalize UI brightness with equal RGB gain; no hue-changing gamma or 332 quantization.
   gain=15/max(1,row['params']['b']);image=image.point([min(255,round(i*gain)) for i in range(256)]*3)
   sheet.paste(image.resize((116,116),Image.Resampling.NEAREST),(x+5+n*122,y+23))
  draw.text((x+5,y+146),'native RGB / normalized brightness',fill='#99a6b3')
 sheet.save(f'/private/tmp/soukan-review-{page+1}.png')
print('Palettes + four review sheets generated.')
