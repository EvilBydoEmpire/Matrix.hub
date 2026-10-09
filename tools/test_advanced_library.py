"""Live revision-3 regression check; restores the user's original selection."""
import json
import pathlib
import urllib.error
from profile_pattern_library import MatrixHttp
root=pathlib.Path(__file__).resolve().parent.parent
snapshot=json.loads((root/'patterns/archive/pre-advanced-2026-10-03/device-defaults.json').read_text())
catalog=json.loads((root/'assets/pattern-catalog.json').read_text())
matrix=MatrixHttp('192.168.178.115')
initial=snapshot['initial']
def select(item):
    matrix.command('setPatternVariant' if item['engine']==16 else 'setPattern',item['variant'] if item['engine']==16 else item['engine'])
try:
    live=matrix.json_get('CONTROL')
    assert live['catalog']==catalog and len(catalog)==45
    assert not any(live['bookmarks'])
    assert live['targetFPS']==initial['targetFPS']
    assert live['autoCycleEnabled']==initial['autoCycleEnabled']
    matrix.command('setAutoCycle',False)
    count=0
    for old in snapshot['patterns']:
        if old['bookmarked']:
            try:select(old)
            except (urllib.error.HTTPError,RuntimeError):pass
            else:raise AssertionError('Deleted pattern still selectable: '+old['name'])
            continue
        select(old);live=matrix.json_get('CONTROL')
        assert live['params']==old['params'],'Changed saved defaults: '+old['name']
        assert live['patternQuality']==old['quality'],'Changed quality: '+old['name']
        count+=1
    for item in catalog[-3:]:
        select(item);live=matrix.json_get('CONTROL')
        assert live['params']['variant']==item['variant']
        assert live['params']['palette']==7 and live['patternQuality']==0
        assert live['params']['b']==(6 if item['variant']==47 else 7)
    print(f'PASS: 7 deleted IDs rejected, {count} survivor defaults/quality unchanged, 3 native-resolution additions; 45 patterns total.')
finally:
    select({'engine':initial['currentPattern'],'variant':initial['params'].get('variant')})
    for key,value in initial['params'].items():
        if key!='variant':matrix.command('setParam',{'animIndex':initial['currentPattern'],'paramName':key,'value':value})
    matrix.command('setFPS',initial['targetFPS'])
    matrix.command('setAutoCycle',initial['autoCycleEnabled'])
