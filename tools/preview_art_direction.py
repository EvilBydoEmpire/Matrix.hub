"""Exact host C++ collection previews; native rows supplied by device capture.

Strobe thumbnails deliberately use one still, not automatic flashing previews.
"""
import json,subprocess
from pathlib import Path
from PIL import Image,ImageDraw
from profile_pattern_library import ASSET,png_bytes
from pack_soukan_preview import pack
root=Path(__file__).resolve().parent.parent
catalog=json.loads((root/'assets/pattern-catalog.json').read_text())
old=Image.open(ASSET).convert('RGB')
atlas=Image.new('RGB',(672,len(catalog)*28));atlas.paste(old.crop((0,0,672,280)),(0,0))
contact=Image.new('RGB',(8*180,((len(catalog)-10+7)//8)*160),'#151719');draw=ImageDraw.Draw(contact)
for i,item in enumerate(catalog):
 if item['engine']!=16:continue
 v=item['variant'];raw=Path(f'/private/tmp/art-preview-{v}.rgb')
 subprocess.run(['/private/tmp/render-art-preview',str(v),str(raw)],check=True)
 data=raw.read_bytes()
 frames=[Image.frombytes('RGB',(64,64),data[j:j+12288]) for j in range(0,len(data),12288)]
 for f in range(24):atlas.paste(frames[120 if v>=65 else 120+f*2].resize((28,28),Image.Resampling.BOX),(f*28,i*28))
 x=((i-10)%8)*180;y=((i-10)//8)*160
 contact.paste(frames[120].resize((128,128),Image.Resampling.NEAREST),(x+24,y))
 draw.text((x+3,y+130),str(v)+' '+item['name'][:24],fill='#eeeeee')
 print(item['name'],flush=True)
ASSET.write_bytes(png_bytes(atlas.width,atlas.height,atlas.tobytes()))
contact.save('/private/tmp/art-direction-contact.png');pack()
