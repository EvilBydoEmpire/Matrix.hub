"""Remap archived preview rows and bootstrap five changed/new native studies."""
import json
import pathlib
import subprocess
from PIL import Image, ImageDraw, ImageFont
from profile_pattern_library import png_bytes, write_header
root=pathlib.Path(__file__).resolve().parent.parent
archive=root/'patterns/archive/pre-advanced-2026-10-03'
catalog=json.loads((root/'assets/pattern-catalog.json').read_text())
old=json.loads((archive/'assets/pattern-catalog.json').read_text())
asset=root/'assets/pattern-preview-atlas.png'
if Image.open(asset).size!=(672,1372):raise RuntimeError('Expected pre-change 49-row atlas; do not replay')
atlas=Image.open(archive/'assets/pattern-preview-atlas.png').convert('RGB')
output=Image.new('RGB',(672,len(catalog)*28))
sheet=Image.new('RGB',(768,5*220),'#101016');draw=ImageDraw.Draw(sheet)
font=ImageFont.truetype('/System/Library/Fonts/Helvetica.ttc',14)
row=0
for index,item in enumerate(catalog):
    if item in old:
        offset=old.index(item)*28;output.paste(atlas.crop((0,offset,672,offset+28)),(0,index*28))
    if item['variant'] not in [43,45,46,47,48]:continue
    raw_path=pathlib.Path(f'/private/tmp/matrix-advanced-{item["variant"]}.rgb')
    subprocess.run(['/private/tmp/render-remix-preview',str(item['variant']),str(raw_path)],check=True)
    raw=raw_path.read_bytes()
    frames=[Image.frombytes('RGB',(64,64),raw[i:i+12288]) for i in range(0,len(raw),12288)]
    draw.text((8,row*220+5),item['name'],font=font,fill='white')
    for j,f in enumerate([30,75,140,200]):sheet.paste(frames[f].resize((184,184),Image.Resampling.NEAREST),(j*192+4,row*220+28))
    for j in range(24):output.paste(frames[90+j*2].resize((28,28),Image.Resampling.BOX),(j*28,index*28))
    row+=1
sheet.save('/private/tmp/matrix-advanced-contact.png')
asset.write_bytes(png_bytes(output.width,output.height,output.tobytes()))
write_header(asset.read_bytes(),28,28,24,len(catalog),67)
print(f'Preserved survivor previews; {len(catalog)} rows with five native-renderer previews.')
