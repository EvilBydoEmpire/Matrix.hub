"""Capture exact removal targets and recoverable defaults before revision 3."""
import json
import pathlib
import shutil
from profile_pattern_library import MatrixHttp

root = pathlib.Path(__file__).resolve().parent.parent
dest = root / 'patterns/archive/pre-advanced-2026-10-03'
if dest.exists():
    raise SystemExit('Archive exists; refusing to overwrite the pre-change snapshot')
matrix = MatrixHttp('192.168.178.115')
initial = matrix.json_get('CONTROL')
rows = []
def select(item):
    matrix.command('setPatternVariant' if item['engine']==16 else 'setPattern',
                   item['variant'] if item['engine']==16 else item['engine'])
try:
    matrix.command('setAutoCycle',False)
    for item, starred in zip(initial['catalog'],initial['bookmarks']):
        select(item)
        state = matrix.json_get('CONTROL')
        rows.append(dict(item, bookmarked=starred, params=state['params'], quality=state['patternQuality']))
finally:
    select({'engine':initial['currentPattern'],'variant':initial['params'].get('variant')})
    for key,value in initial['params'].items():
        if key!='variant':matrix.command('setParam',{'animIndex':initial['currentPattern'],'paramName':key,'value':value})
    matrix.command('setAutoCycle',initial['autoCycleEnabled'])
dest.mkdir(parents=True)
# Only rendering/library source, no network credentials or unrelated files.
for file in ['64x64_matrix_01.ino','src/patterns/PatternflowPack.cpp','src/patterns/PatternflowPack.h',
             'src/patterns/PatternRemixes.inc','src/patterns/WhitneyStudies.inc',
             'src/patterns/PatternflowImports.inc','src/patterns/LibrarySettings.inc',
             'src/generated/PatternCatalog.h','assets/pattern-catalog.json','assets/pattern-preview-atlas.png']:
    target=dest/file;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(root/file,target)
safe_initial={k:initial[k] for k in ['currentPattern','params','targetFPS','autoCycleEnabled','bookmarks','catalog']}
(dest/'device-defaults.json').write_text(json.dumps({'initial':safe_initial,'patterns':rows},indent=2)+'\n')
for row in rows:
    if row['bookmarked']:print('REMOVE:',row['name'],row['variant'])
    if row['engine']==1 or row['variant'] in [33,38,39,43,45]:print('REFERENCE:',row['name'],row['params'])
