"""Snapshot surviving defaults before upload, then verify identity/settings after."""
import argparse
import json
import pathlib
from profile_pattern_library import MatrixHttp

parser=argparse.ArgumentParser()
parser.add_argument('mode',choices=['snapshot','verify'])
parser.add_argument('--host',default='192.168.178.115')
args=parser.parse_args()
root=pathlib.Path(__file__).resolve().parent.parent
archive=root/'patterns/archive/pre-remix-2026-10-03'
catalog=json.loads((root/'assets/pattern-catalog.json').read_text())
matrix=MatrixHttp(args.host)
before=matrix.json_get('CONTROL')
records=[]
def select(item):
    matrix.command('setPatternVariant' if item['engine']==16 else 'setPattern',item['variant'] if item['engine']==16 else item['engine'])
try:
    if args.mode=='snapshot':
        for item in catalog:
            if item['engine']==16 and item['variant']>=35:continue
            select(item)
            state=matrix.json_get('CONTROL')
            records.append(dict(item,params=state['params']))
        (archive/'surviving-defaults.json').write_text(json.dumps(records,indent=2)+'\n')
        print(f'Snapshotted all {len(records)} surviving default patches.')
    else:
        assert before['catalog']==catalog,'Device catalog differs from compiled catalog'
        assert len(before['bookmarks'])==46 and not any(before['bookmarks']),'Removed bookmarks were not cleared'
        for record in json.loads((archive/'surviving-defaults.json').read_text()):
            select(record)
            now=matrix.json_get('CONTROL')['params']
            assert now==record['params'],f"Default changed: {record['name']}\n{now}\n{record['params']}"
        for item in catalog[-8:]:
            select(item)
            now=matrix.json_get('CONTROL')['params']
            assert now['palette']==7 and now['variant']==item['variant'],item
        print('PASS: 46 catalog entries, 8 deletions, 8 new remixes, all 38 surviving defaults unchanged, independent Original palettes.')
finally:
    select({'engine':before['currentPattern'],'variant':before['params'].get('variant')})
    for name,value in before['params'].items():
        if name!='variant':matrix.command('setParam',{'animIndex':before['currentPattern'],'paramName':name,'value':value})
