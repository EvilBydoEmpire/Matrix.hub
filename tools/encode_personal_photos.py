"""Encode already-resampled user photos as RGB444 flash textures, no dithering.

PRIVATE: generated textures and preview atlas contain personal photographs.
Do not publish the firmware/assets to a public repository without consent.
The originals are untouched; macOS ImageIO/sips handles HEIC and orientation.
"""
from pathlib import Path
from io import BytesIO
from PIL import Image, ImageCms
root=Path(__file__).resolve().parent.parent
lines=['#pragma once','// PRIVATE USER PHOTOS: local/device use only; not cleared for public distribution.',
       'const uint16_t PERSONAL_PHOTOS[2][4096] = {']
for name in ['4786','4704']:
    im=Image.open(f'/private/tmp/matrix-photo-{name}-64.png')
    # iPhone HEICs use Display P3: convert the embedded profile rather than
    # interpreting wide-gamut values as sRGB and shifting skin/foliage colors.
    profile=im.info.get('icc_profile')
    if profile:im=ImageCms.profileToProfile(im,ImageCms.ImageCmsProfile(BytesIO(profile)),ImageCms.createProfile('sRGB'),outputMode='RGB')
    else:im=im.convert('RGB')
    assert im.size==(64,64)
    pixels=im.get_flattened_data() if hasattr(im,'get_flattened_data') else im.getdata()
    values=[((r>>4)<<8)|((g>>4)<<4)|(b>>4) for r,g,b in pixels]
    lines.append(' { // IMG_'+name)
    for i in range(0,4096,16):lines.append('  '+','.join(f'0x{v:03x}' for v in values[i:i+16])+',')
    lines.append(' },')
lines.append('};')
(root/'src/generated/PersonalPhotos.h').write_text('\n'.join(lines)+'\n')
print('Encoded two 64×64 photos: 16 KiB flash total; no full-resolution originals copied.')
