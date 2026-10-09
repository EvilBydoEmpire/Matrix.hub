"""Offline native-renderer QA; safely append three Whitney atlas rows once."""
import json
import pathlib
import subprocess
from PIL import Image, ImageDraw, ImageFont
from profile_pattern_library import png_bytes, write_header
ROOT=pathlib.Path(__file__).resolve().parent.parent
asset=ROOT/'assets/pattern-preview-atlas.png'
atlas=Image.open(asset).convert('RGB')
if atlas.size!=(28*24,28*46):
    raise RuntimeError('Expected pre-Whitney atlas: do not replace real-device recordings by rerunning this bootstrap.')
output=Image.new('RGB',(28*24,28*49));output.paste(atlas,(0,0))
catalog=json.loads((ROOT/'assets/pattern-catalog.json').read_text())
sheet=Image.new('RGB',(4*192,3*222),'#111118');draw=ImageDraw.Draw(sheet)
font=ImageFont.truetype('/System/Library/Fonts/Helvetica.ttc',14)
for row,v in enumerate(range(43,46)):
    raw_path=pathlib.Path(f'/private/tmp/matrix-whitney-{v}.rgb')
    subprocess.run(['/private/tmp/render-remix-preview',str(v),str(raw_path)],check=True)
    raw=raw_path.read_bytes()
    frames=[Image.frombytes('RGB',(64,64),raw[i:i+64*64*3]) for i in range(0,len(raw),64*64*3)]
    draw.text((8,row*222+6),catalog[46+row]['name'],font=font,fill='white')
    for n,k in enumerate([30,75,140,200]):
        sheet.paste(frames[k].resize((184,184),Image.Resampling.NEAREST),(n*192+4,row*222+30))
    frames[0].save(f'/private/tmp/matrix-whitney-{v}.gif',save_all=True,append_images=frames[1:],duration=33,loop=0)
    # Preview bootstrap uses the exact native algorithms. The subsequent device
    # capture replaces these rows with the actual brightness-aware final output.
    for k in range(24):
        thumbnail=frames[30+k*2].resize((28,28),Image.Resampling.BOX)
        channels=thumbnail.split()
        thumbnail=Image.merge('RGB',tuple(channel.point([round(round(value*steps/255)*255/steps) for value in range(256)]) for channel,steps in zip(channels,(7,7,3))))
        output.paste(thumbnail,(k*28,(46+row)*28))
sheet.save('/private/tmp/matrix-whitney-contact.png')
asset.write_bytes(png_bytes(output.width,output.height,output.tobytes()))
write_header(asset.read_bytes(),28,28,24,49,67)
print('Rendered three Whitney studies, preserved existing 46 preview rows, appended three native-rendered rows.')
