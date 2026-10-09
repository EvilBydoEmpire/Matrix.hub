"""Verify append-only library updates without resetting bookmarks or defaults."""
import argparse
import json
import pathlib
from profile_pattern_library import MatrixHttp
parser=argparse.ArgumentParser()
parser.add_argument('mode',choices=['snapshot','verify'])
parser.add_argument('--host',default='192.168.178.115')
parser.add_argument('--snapshot',type=pathlib.Path,required=True)
args=parser.parse_args()
matrix=MatrixHttp(args.host)
root=pathlib.Path(__file__).resolve().parent.parent
catalog=json.loads((root/'assets/pattern-catalog.json').read_text())
def select(item):
    matrix.command('setPatternVariant' if item['engine']==16 else 'setPattern',item['variant'] if item['engine']==16 else item['engine'])
def restore(state):
    select({'engine':state['currentPattern'],'variant':state['params'].get('variant')})
    for name,value in state['params'].items():
        if name!='variant':matrix.command('setParam',{'animIndex':state['currentPattern'],'paramName':name,'value':value})
if args.mode=='snapshot':
    initial=matrix.json_get('CONTROL');old=initial['catalog']
    assert catalog[:len(old)]==old and len(catalog)>len(old),'Expected append-only catalog update'
    rows=[]
    try:
        for item in old:
            select(item);state=matrix.json_get('CONTROL')
            rows.append(dict(item,params=state['params'],quality=state['patternQuality']))
        args.snapshot.write_text(json.dumps({'initial':initial,'patterns':rows},indent=2)+'\n')
        print(f'Snapshotted {len(rows)} existing defaults and bookmarks.')
    finally:restore(initial)
else:
    saved=json.loads(args.snapshot.read_text());initial=saved['initial']
    state=matrix.json_get('CONTROL');old_count=len(initial['catalog'])
    try:
        assert state['catalog']==catalog
        assert state['bookmarks'][:old_count]==initial['bookmarks']
        assert not any(state['bookmarks'][old_count:])
        assert state['targetFPS']==initial['targetFPS'] and state['autoCycleEnabled']==initial['autoCycleEnabled']
        for row in saved['patterns']:
            select(row);now=matrix.json_get('CONTROL')
            assert now['params']==row['params'],f"Defaults changed: {row['name']}"
            assert now['patternQuality']==row['quality'],f"Quality changed: {row['name']}"
        for row in catalog[old_count:]:
            select(row);now=matrix.json_get('CONTROL')
            assert now['params']['variant']==row['variant'] and now['params']['palette']==7
            assert now['patternQuality']==0
        print(f'PASS: {len(catalog)-old_count} new patterns; all {old_count} original defaults, quality choices, bookmarks, target FPS and auto-cycle preserved.')
    finally:restore(initial)
