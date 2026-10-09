"""Temporary new-remix patch/bookmark, verified across final OTA then restored."""
import argparse
import json
import pathlib
import urllib.error
from profile_pattern_library import MatrixHttp

parser=argparse.ArgumentParser()
parser.add_argument('mode',choices=['prepare','finish'])
parser.add_argument('--host',default='192.168.178.115')
args=parser.parse_args()
matrix=MatrixHttp(args.host)
checkpoint=pathlib.Path('/private/tmp/matrix-remix-persistence.json')
def select(state):
    matrix.command('setPatternVariant' if state['currentPattern']==16 else 'setPattern',state['params']['variant'] if state['currentPattern']==16 else state['currentPattern'])
def apply(params,engine=16):
    for name,value in params.items():
        if name!='variant':matrix.command('setParam',{'animIndex':engine,'paramName':name,'value':value})
if args.mode=='prepare':
    initial=matrix.json_get('CONTROL')
    assert not any(initial['bookmarks']),'Unexpected new user bookmarks: do not overwrite them'
    matrix.command('setPatternVariant',35)
    original=matrix.json_get('CONTROL')['params']
    checkpoint.write_text(json.dumps({'initial':initial,'original':original},indent=2)+'\n')
    apply({'c0':73,'b':8,'palette':9,'color1':'#12AB34','lfo_target':1,'lfo_speed':.75,'lfo_amount':.32})
    matrix.command('saveCurrentDefault',0)
    expected=matrix.json_get('CONTROL')['params']
    data=json.loads(checkpoint.read_text());data['expected']=expected
    checkpoint.write_text(json.dumps(data,indent=2)+'\n')
    matrix.command('setBookmark',{'index':38,'enabled':True})
    matrix.command('setPatternVariant',36)
    sibling=matrix.json_get('CONTROL')['params']
    assert sibling['palette']==7 and sibling['c0']==30 and sibling['lfo_target']==0
    print('Prepared isolated persistence probe on new Whitney remix; originals untouched.')
else:
    data=json.loads(checkpoint.read_text())
    try:
        state=matrix.json_get('CONTROL')
        assert state['bookmarks'][38] and sum(state['bookmarks'])==1,'Bookmark lost/mis-mapped on reboot'
        matrix.command('setPatternVariant',35)
        state=matrix.json_get('CONTROL')
        assert state['params']==data['expected'],'New variant patch/palette/LFO did not persist'
        for command,value in [('setPattern',10),('setPatternVariant',17),('setPatternVariant',20),('setPatternVariant',32),('setParam',{'animIndex':16,'paramName':'variant','value':17})]:
            try:matrix.command(command,value)
            except urllib.error.HTTPError as error:assert error.code==400
            else:raise AssertionError(f'Retired pattern still selectable: {command} {value}')
        assert matrix.json_get('CONTROL')['params']['variant']==35
        print('PASS: saved defaults, editable palette, LFO and bookmarks survive reboot; retired API IDs rejected.')
    finally:
        matrix.command('setPatternVariant',35)
        apply(data['original']);matrix.command('saveCurrentDefault',0)
        matrix.command('setBookmark',{'index':38,'enabled':False})
        select(data['initial']);apply(data['initial']['params'],data['initial']['currentPattern'])
        print('All temporary settings and the test bookmark restored.')
