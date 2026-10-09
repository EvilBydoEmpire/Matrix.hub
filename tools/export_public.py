#!/usr/bin/env python3
"""Export a fresh public source snapshot, replacing private photos AND previews.

Never git-add the private working tree wholesale. Export into an empty directory,
compile/test it, inspect its diff in a clean Git checkout, then publish.
"""
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
PRIVATE_NAMES = ('Foto — IMG_4786', 'Foto — IMG_4704')
PUBLIC_NAMES = ('Photo — Empty slot 1', 'Photo — Empty slot 2')

def export(destination):
    destination = destination.resolve()
    if destination == ROOT or ROOT in destination.parents:
        raise ValueError('Export outside the private source tree')
    destination.mkdir(parents=True, exist_ok=True)
    if any(destination.iterdir()):
        raise ValueError('Destination must be empty; never overwrite a checkout')
    files = [ROOT / name for name in ('64x64_matrix_01.ino', 'platformio.ini', 'README.md', 'THIRD_PARTY_NOTICES.md', '.gitignore')]
    for name in ('LICENSE', 'LICENSE.md', 'requirements-dev.txt', 'patterns/README.md'):
        if (ROOT/name).exists(): files.append(ROOT/name)
    allowed = {'.h', '.hpp', '.c', '.cpp', '.inc', '.ini', '.csv', '.ino', '.properties', '.md', '.txt', '.js', '.json', '.lua'}
    for directory in ('src', 'lib/lua', 'arduino', 'partitions', 'catalog', 'patterns/examples', 'patterns/imports', 'gift'):
        for p in (ROOT/directory).rglob('*'):
            if not p.is_file() or p.is_symlink(): continue
            if any(part in ('graphify-out', '__pycache__', '.pio', '.git') for part in p.relative_to(ROOT).parts): continue
            if p.name in ('PersonalPhotos.h', 'PatternPreviewAtlas.h'): continue
            if p.suffix in allowed or p.name in ('LICENSE', 'README', 'Makefile'): files.append(p)
    files += list((ROOT/'docs').glob('*.md'))
    # Top-level source tools only: no audio recordings/apps, caches or binaries.
    for p in (ROOT/'tools').iterdir():
        if p.is_file() and p.suffix in ('.py', '.cjs', '.cpp', '.command'):
            files.append(p)
    files += list((ROOT/'tools/test-support').glob('*.h'))
    for p in files:
        relative = p.relative_to(ROOT)
        target = destination/relative
        target.parent.mkdir(parents=True, exist_ok=True)
        content = p.read_text()
        # Keep stable variant IDs; public photo slots explicitly contain no image.
        if p.suffix in ('.cpp', '.ino'):
            for old, new in zip(PRIVATE_NAMES, PUBLIC_NAMES): content = content.replace(old, new)
        target.write_text(content)
        shutil.copymode(p, target)
    generated = destination/'src/generated'
    generated.mkdir(parents=True, exist_ok=True)
    (generated/'PersonalPhotos.h').write_text(
        '#pragma once\n// Public build: deliberately empty. No personal image data.\n'
        'const uint16_t PERSONAL_PHOTOS[2][4096] = {};\n')
    catalog = json.loads((ROOT/'assets/pattern-catalog.json').read_text())
    photo_rows = []
    for row, pattern in enumerate(catalog):
        if pattern.get('engine') == 16 and pattern.get('variant') in (56, 57):
            photo_rows.append(row)
            pattern['name'] = PUBLIC_NAMES[pattern['variant'] - 56]
    if len(photo_rows) != 2: raise ValueError('Photo catalog changed; review privacy export')
    assets = destination/'assets'
    assets.mkdir(parents=True, exist_ok=True)
    atlas = Image.open(ROOT/'assets/pattern-preview-atlas.png').convert('RGB')
    if atlas.height != len(catalog)*28 or atlas.width != 24*28:
        raise ValueError('Preview atlas layout changed; review before publishing')
    for row in photo_rows:
        atlas.paste((0, 0, 0), (0, row*28, atlas.width, (row+1)*28))
    atlas.save(assets/'pattern-preview-atlas.png', optimize=True)
    (assets/'pattern-catalog.json').write_text(json.dumps(catalog, indent=2)+'\n')
    # Rebuild all derived image bytes/header; never copy the private derivatives.
    subprocess.run([sys.executable, str(destination/'tools/pack_soukan_preview.py')], cwd=destination, check=True)
    for name in ('pattern-preview-atlas.png', 'pattern-preview-soukan-hub75.png', 'pattern-preview-soukan-ws2812.png'):
        image = Image.open(assets/name).convert('RGB')
        size = image.height//len(catalog)
        for row in photo_rows:
            if any(hi for lo, hi in image.crop((0, row*size, image.width, (row+1)*size)).getextrema()):
                raise RuntimeError('Private photo preview row not fully removed')
    (destination/'PUBLIC_BUILD.txt').write_text(
        'Matrix.hub by Televator — public source snapshot.\n'
        'Personal photo arrays replaced with zero data; both preview rows are black.\n'
        'All derived preview headers regenerated from the scrubbed source atlas.\n'
        'No full-flash backups, credentials, local graphs, recordings or build binaries included.\n'
        'Photo slot IDs remain stable to preserve device settings compatibility.\n')
    manifest = {str(p.relative_to(destination)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in sorted(destination.rglob('*')) if p.is_file() and '__pycache__' not in p.parts}
    # Importing the encoder may create bytecode. Remove only its generated cache
    # from this newly created export directory, never anything in the source tree.
    cache = destination/'tools/__pycache__'
    if cache.exists(): shutil.rmtree(cache)
    (destination/'PUBLIC_MANIFEST.json').write_text(json.dumps(manifest, indent=2)+'\n')
    print('Public snapshot:', destination, 'files:', len(manifest), 'scrubbed rows:', photo_rows)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', type=Path)
    export(parser.parse_args().destination)
