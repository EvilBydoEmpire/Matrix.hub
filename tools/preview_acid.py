"""Append exact C++ simulation previews, retaining all earlier atlas rows."""
from pathlib import Path
import subprocess,json
from PIL import Image,ImageDraw
from profile_pattern_library import png_bytes,write_header
root=Path(__file__).resolve().parent.parent
asset=root/'assets/pattern-preview-atlas.png'
old=Image.open(asset).convert('RGB')
assert old.size in [(672,56*28),(672,59*28)]
catalog=json.loads((root/'assets/pattern-catalog.json').read_text())
assert len(catalog)==59
out=Image.new('RGB',(672,59*28));out.paste(old,(0,0))
sheet=Image.new('RGB',(768,3*208),'#101016')
for row,v in enumerate([60,61,62]):
    raw=Path(f'/private/tmp/acid-{v}.rgb')
    subprocess.run(['/private/tmp/render-remix-preview',str(v),str(raw)],check=True)
    data=raw.read_bytes();frames=[Image.frombytes('RGB',(64,64),data[i:i+12288]) for i in range(0,len(data),12288)]
    index=next(i for i,x in enumerate(catalog) if x.get('variant')==v)
    for i in range(24):out.paste(frames[60+i*2].resize((28,28),Image.Resampling.BOX),(i*28,index*28))
    ImageDraw.Draw(sheet).text((6,row*208+3),catalog[index]['name'],fill='white')
    for i,f in enumerate([30,75,140,220]):sheet.paste(frames[f].resize((184,184),Image.Resampling.NEAREST),(i*192+4,row*208+20))
    frames[0].save(f'/private/tmp/acid-{v}.gif',save_all=True,append_images=frames[1:],duration=33,loop=0)
asset.write_bytes(png_bytes(out.width,out.height,out.tobytes()))
write_header(asset.read_bytes(),28,28,24,59,67)
sheet.save('/private/tmp/acid-contact.png')
print('59 previews: three acid studies appended, earlier 56 retained.')
