"""One-time native-render preview bootstrap for seven studies + two photos."""
from pathlib import Path
import json,subprocess
from PIL import Image,ImageDraw,ImageFont
from profile_pattern_library import png_bytes,write_header
root=Path(__file__).resolve().parent.parent
asset=root/'assets/pattern-preview-atlas.png'
atlas=Image.open(asset).convert('RGB')
if atlas.size!=(672,45*28):raise RuntimeError('Expected 45-row starting atlas; do not replay')
catalog=json.loads((root/'assets/pattern-catalog.json').read_text())
assert len(catalog)==54
output=Image.new('RGB',(672,54*28));output.paste(atlas,(0,0))
sheets=[Image.new('RGB',(768,n*218),'#101016') for n in [5,4]]
font=ImageFont.truetype('/System/Library/Fonts/Helvetica.ttc',14)
for index in range(45,54):
    item=catalog[index];v=item['variant'];file=Path(f'/private/tmp/matrix-nova-{v}.rgb')
    subprocess.run(['/private/tmp/render-remix-preview',str(v),str(file)],check=True)
    raw=file.read_bytes();frames=[Image.frombytes('RGB',(64,64),raw[i:i+12288]) for i in range(0,len(raw),12288)]
    sheet=sheets[(index-45)//5];row=(index-45)%5
    ImageDraw.Draw(sheet).text((8,row*218+5),item['name'],font=font,fill='white')
    for j,f in enumerate([30,75,140,200]):sheet.paste(frames[f].resize((184,184),Image.Resampling.NEAREST),(j*192+4,row*218+26))
    for j in range(24):output.paste(frames[90+j*2].resize((28,28),Image.Resampling.BOX),(j*28,index*28))
    if v<56:frames[0].save(f'/private/tmp/matrix-nova-{v}.gif',save_all=True,append_images=frames[1:],duration=33,loop=0)
for i,sheet in enumerate(sheets):sheet.save(f'/private/tmp/matrix-nova-sheet-{i}.png')
asset.write_bytes(png_bytes(output.width,output.height,output.tobytes()))
write_header(asset.read_bytes(),28,28,24,54,67)
print('Nine exact-renderer previews appended; 45 existing rows preserved. PRIVATE photos included.')
