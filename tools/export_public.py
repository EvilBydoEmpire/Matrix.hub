#!/usr/bin/env python3
"""Export a fresh public source snapshot, excluding local state and backups.

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
        target.write_text(content)
        shutil.copymode(p, target)
    generated = destination/'src/generated'
    generated.mkdir(parents=True, exist_ok=True)
    catalog = json.loads((ROOT/'assets/pattern-catalog.json').read_text())
    if any(p.get('engine')==16 and p.get('variant') in (56,57) for p in catalog):
        raise ValueError('Retired personal-photo entries must not be exported')
    if (ROOT/'src/generated/PersonalPhotos.h').exists():
        raise ValueError('Unexpected personal-photo header; review before publishing')
    assets = destination/'assets'
    assets.mkdir(parents=True, exist_ok=True)
    atlas = Image.open(ROOT/'assets/pattern-preview-atlas.png').convert('RGB')
    if atlas.height != len(catalog)*28 or atlas.width != 24*28:
        raise ValueError('Preview atlas layout changed; review before publishing')
    atlas.save(assets/'pattern-preview-atlas.png', optimize=True)
    (assets/'pattern-catalog.json').write_text(json.dumps(catalog, indent=2)+'\n')
    # Rebuild all derived image bytes/header; never copy the private derivatives.
    subprocess.run([sys.executable, str(destination/'tools/pack_soukan_preview.py')], cwd=destination, check=True)
    (destination/'PUBLIC_BUILD.txt').write_text(
        'Matrix.hub by Televator — public source snapshot.\n'
        'Personal photo patterns, textures and preview rows removed entirely.\n'
        f'All derived preview headers regenerated from the current {len(catalog)}-pattern atlas.\n'
        'No full-flash backups, credentials, local graphs, recordings or build binaries included.\n'
        'Retired IDs remain reserved internally, not visible as empty gallery slots.\n')
    manifest = {str(p.relative_to(destination)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in sorted(destination.rglob('*')) if p.is_file() and '__pycache__' not in p.parts}
    # Importing the encoder may create bytecode. Remove only its generated cache
    # from this newly created export directory, never anything in the source tree.
    cache = destination/'tools/__pycache__'
    if cache.exists(): shutil.rmtree(cache)
    (destination/'PUBLIC_MANIFEST.json').write_text(json.dumps(manifest, indent=2)+'\n')
    print('Public snapshot:', destination, 'files:', len(manifest), 'patterns:',len(catalog))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', type=Path)
    export(parser.parse_args().destination)
