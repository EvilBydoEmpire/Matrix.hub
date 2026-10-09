"""Append actual C++ render samples, retaining every existing preview row."""
from pathlib import Path
import subprocess,json
from PIL import Image,ImageDraw
from profile_pattern_library import ASSET,png_bytes
from pack_soukan_preview import pack
root=Path(__file__).resolve().parent.parent
old=Image.open(ASSET).convert('RGB')
assert old.size in [(672,59*28),(672,61*28)]
out=Image.new('RGB',(672,61*28));out.paste(old.crop((0,0,672,59*28)),(0,0))
sheet=Image.new('RGB',(768,416),'#101016');draw=ImageDraw.Draw(sheet)
for row,v in enumerate((63,64)):
 raw=Path(f'/private/tmp/classic08-{v}.rgb')
 subprocess.run(['/private/tmp/render-classic08-preview',str(v),str(raw),'soukan'],check=True)
 data=raw.read_bytes();frames=[Image.frombytes('RGB',(64,64),data[i:i+12288]) for i in range(0,len(data),12288)]
 for i in range(24):out.paste(frames[120+i*2].resize((28,28),Image.Resampling.BOX),(i*28,(59+row)*28))
 draw.text((6,row*208+3),'Ripples Classic 08' if v==63 else 'Plasma Classic 08',fill='white')
 for i,f in enumerate((60,120,180,239)):sheet.paste(frames[f].resize((184,184),Image.Resampling.NEAREST),(i*192+4,row*208+20))
ASSET.write_bytes(png_bytes(out.width,out.height,out.tobytes()))
sheet.save('/private/tmp/classic08-contact.png');pack()
