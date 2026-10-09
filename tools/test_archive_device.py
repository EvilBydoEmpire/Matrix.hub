"""Reversible archive check, including a real reboot between stage and verify."""
import argparse,json,urllib.request,urllib.error
from pathlib import Path
from profile_pattern_library import MatrixHttp
p=argparse.ArgumentParser();p.add_argument('mode',choices=['snapshot','stage','verify']);p.add_argument('--host',default='192.168.178.115');a=p.parse_args()
m=MatrixHttp(a.host);file=Path('/private/tmp/matrix-archive-test.json')
def row(lib,id):return next((r for r in lib['entries'] if r[0]==id),[id,0,0,0,0,False])
def post(command,value):
    req=urllib.request.Request(m.base+'/control',json.dumps({'command':command,'value':value}).encode(),{'Content-Type':'application/json'})
    try:
        with urllib.request.urlopen(req,timeout=10) as r:return r.status,json.load(r)
    except urllib.error.HTTPError as e:return e.code,json.load(e)
state=m.json_get('CONTROL');lib=m.json_get('LIBRARY')
if a.mode=='snapshot':
    assert len(state['catalog'])==56
    keep={k:state[k] for k in ['catalog','bookmarks','targetFPS','autoCycleEnabled','currentPattern','params']}
    file.write_text(json.dumps({'before':keep,'library':lib},indent=2)+'\n');print('Snapshot saved without network credentials.')
else:
    saved=json.loads(file.read_text());before=saved['before'];assert len(state['catalog'])==59
    assert state['catalog'][:56]==before['catalog'] and state['bookmarks'][:56]==before['bookmarks']
    if a.mode=='stage':
        assert [r[:5] for r in lib['entries']]==[r[:5] for r in saved['library']['entries']]
        m.command('setAutoCycle',False)
        m.command('setPatternVariant',60)
        m.command('setPatternArchived',{'id':76,'archived':True})
        assert row(m.json_get('LIBRARY'),76)[5] is True
        after=m.json_get('CONTROL');assert after['params']['variant']==61,'current archived pattern did not advance'
        code,result=post('setPatternVariant',60);assert code==400 and not result['success']
        for invalid in [{'id':-1,'archived':True},{'id':79,'archived':True},{'id':76,'archived':1},{'id':True,'archived':True}]:
            code,result=post('setPatternArchived',invalid);assert code==400 and not result['success'],invalid
        # Editing organization must not clear the archive flag.
        code,result=post('setLibraryMetadata',{'id':76,'rating':0,'folder':'','family':'','tags':[]})
        assert code==200 and row(m.json_get('LIBRARY'),76)[5] is True
        m.command('setPatternArchived',{'id':76,'archived':False})
        m.command('setPatternVariant',60)
        assert not row(m.json_get('LIBRARY'),76)[5]
        m.command('setPatternArchived',{'id':76,'archived':True})
        print('PASS archive/restore, active-pattern advance, blocked archived selection, strict validation and preserved metadata. Archive staged for reboot test.')
    else:
        assert row(lib,76)[5] is True,'archive lost on reboot'
        m.command('setPatternArchived',{'id':76,'archived':False})
        lib=m.json_get('LIBRARY');assert [r[:5] for r in lib['entries']]==[r[:5] for r in saved['library']['entries']]
        m.command('setPatternVariant' if before['currentPattern']==16 else 'setPattern',before['params']['variant'] if before['currentPattern']==16 else before['currentPattern'])
        for key,value in before['params'].items():
            if key!='variant':m.command('setParam',{'animIndex':before['currentPattern'],'paramName':key,'value':value})
        assert m.json_get('CONTROL')['params']==before['params']
        m.command('setFPS',before['targetFPS']);m.command('setAutoCycle',before['autoCycleEnabled'])
        report={'passed':['append-only catalog','bookmarks and metadata preserved','archive current advances playback','archive blocks manual selection','restore','metadata edit preserves archive','strict API validation','archive survives OTA reboot','original playback restored'],'diagnostics':{k:v for k,v in m.json_get('DIAGNOSTICS').items() if k in ['freeHeap','minimumFreeHeap','panelRefreshHz','colorDepth','displayClockMHz']}}
        (Path(__file__).resolve().parent.parent/'docs/archive-verification.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report,indent=2))
