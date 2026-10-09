"""Visual QA and initial gallery atlas from the exact compiled C++ renderer."""
import json
import pathlib
import subprocess
from PIL import Image, ImageDraw, ImageFont
from profile_pattern_library import write_header, png_bytes

ROOT = pathlib.Path(__file__).resolve().parent.parent
catalog = json.loads((ROOT / 'assets/pattern-catalog.json').read_text())
archive = ROOT / 'patterns/archive/pre-remix-2026-10-03'
old_catalog = json.loads((archive / 'pattern-catalog.json').read_text())
old_atlas = Image.open(archive / 'pattern-preview-atlas.png').convert('RGB')
atlas = Image.new('RGB', (28 * 24, 28 * len(catalog)))
for row, item in enumerate(catalog):
    key = (item['engine'], item['variant'])
    source = next((j for j, old in enumerate(old_catalog) if (old['engine'], old['variant']) == key), None)
    if source is not None:
        atlas.paste(old_atlas.crop((0, source * 28, 24 * 28, (source + 1) * 28)), (0, row * 28))

sheet = Image.new('RGB', (4 * 320, 2 * 238), '#111118')
draw = ImageDraw.Draw(sheet)
font = ImageFont.truetype('/System/Library/Fonts/Helvetica.ttc', 14)
for n, variant in enumerate(range(35, 43)):
    filename = pathlib.Path(f'/private/tmp/matrix-remix-{variant}.rgb')
    subprocess.run(['/private/tmp/render-remix-preview', str(variant), str(filename)], check=True)
    raw = filename.read_bytes()
    frames = [Image.frombytes('RGB', (64, 64), raw[i:i+64*64*3]) for i in range(0, len(raw), 64*64*3)]
    name = next(item['name'] for item in catalog if item['variant'] == variant)
    x, y = (n % 4) * 320, (n // 4) * 238
    draw.text((x + 8, y + 8), name, font=font, fill='white')
    for j, frame_index in enumerate([45, 120]):
        sheet.paste(frames[frame_index].resize((152, 152), Image.Resampling.NEAREST), (x + 8 + j*156, y + 33))
    frames[0].save(f'/private/tmp/matrix-remix-{variant}.gif', save_all=True, append_images=frames[1:], duration=33, loop=0)
    row = next(j for j, item in enumerate(catalog) if item['variant'] == variant)
    for k in range(24):
        thumbnail=frames[30+k*2].resize((28, 28), Image.Resampling.BOX)
        channels=thumbnail.split()
        thumbnail=Image.merge('RGB', tuple(channel.point([round(round(v*steps/255)*255/steps) for v in range(256)]) for channel,steps in zip(channels,(7,7,3))))
        atlas.paste(thumbnail, (k*28, row*28))
sheet.save('/private/tmp/matrix-remix-contact.png')
asset = ROOT / 'assets/pattern-preview-atlas.png'
asset.write_bytes(png_bytes(atlas.width, atlas.height, atlas.tobytes()))
write_header(asset.read_bytes(), 28, 28, 24, len(catalog), 67)
print('Rendered eight exact-C++ animations and rebuilt catalog-aligned preview atlas.')
