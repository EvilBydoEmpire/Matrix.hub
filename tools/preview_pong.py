"""Guarded native-render bootstrap; final preview is captured on the ESP32."""
from pathlib import Path
import subprocess
from PIL import Image,ImageDraw
from profile_pattern_library import png_bytes,write_header
root=Path(__file__).resolve().parent.parent
asset=root/'assets/pattern-preview-atlas.png'
old=Image.open(asset).convert('RGB')
if old.size!=(672,54*28):raise RuntimeError('Expected 54-row atlas; do not replay')
raw_file=Path('/private/tmp/pong-preview.rgb')
subprocess.run(['/private/tmp/render-remix-preview','58',str(raw_file)],check=True)
raw=raw_file.read_bytes();frames=[Image.frombytes('RGB',(64,64),raw[i:i+12288]) for i in range(0,len(raw),12288)]
output=Image.new('RGB',(672,55*28));output.paste(old,(0,0))
for i in range(24):output.paste(frames[60+i*2].resize((28,28),Image.Resampling.BOX),(i*28,54*28))
asset.write_bytes(png_bytes(output.width,output.height,output.tobytes()))
write_header(asset.read_bytes(),28,28,24,55,67)
sheet=Image.new('RGB',(768,208),'#111119')
for i,f in enumerate([0,60,120,210]):sheet.paste(frames[f].resize((184,184),Image.Resampling.NEAREST),(i*192+4,20))
ImageDraw.Draw(sheet).text((8,3),'Pong Wars — native 64x64 renderer',fill='white')
sheet.save('/private/tmp/matrix-pong-contact.png')
frames[0].save('/private/tmp/matrix-pong.gif',save_all=True,append_images=frames[1:],duration=33,loop=0)
print('Pong preview appended; all 54 previous rows unchanged.')
