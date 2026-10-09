"""Reversible library API/persistence checks. No pattern presets are saved.

snapshot before deployment; stage after deployment; verify after next reboot.
Only stage creates a temporary record; verify restores that record exactly.
"""
import argparse,json,urllib.request,urllib.error
from pathlib import Path
from profile_pattern_library import MatrixHttp
p=argparse.ArgumentParser();p.add_argument('mode',choices=['snapshot','recover-test','stage','verify']);p.add_argument('--host',default='192.168.178.115');p.add_argument('--file',type=Path,default=Path('/private/tmp/matrix-organizer-test.json'));a=p.parse_args()
m=MatrixHttp(a.host)
def post(value):
    request=urllib.request.Request(m.base+'/control',json.dumps({'command':'setLibraryMetadata','value':value}).encode(),{'Content-Type':'application/json'})
    try:
        with urllib.request.urlopen(request,timeout=10) as r:return r.status,json.load(r)
    except urllib.error.HTTPError as e:return e.code,json.load(e)
def expanded(lib,id):
    r=next((row for row in lib['entries'] if row[0]==id),[id,0,0,0,0])
    return {'id':id,'rating':r[1],'folder':lib['folders'][r[2]-1] if r[2] else '',
            'family':lib['families'][r[3]-1] if r[3] else '',
            'tags':[t for i,t in enumerate(lib['tags']) if r[4]&(1<<i)]}
state=m.json_get('CONTROL')
if a.mode=='snapshot':
    assert len(state['catalog'])==55
    keep={k:state[k] for k in ['catalog','bookmarks','targetFPS','autoCycleEnabled','currentPattern','params','patternQuality']}
    a.file.write_text(json.dumps({'before':keep},indent=2)+'\n');print('Saved pre-update catalog/bookmarks/playback snapshot (no network credentials).')
elif a.mode=='recover-test':
    saved=json.loads(a.file.read_text());lib=m.json_get('LIBRARY')
    assert saved['original']=={'id':75,'rating':0,'folder':'','family':'','tags':[]}
    assert not lib['readable'] and lib['storedLength']==2232 and lib['storageSlot']==255
    assert lib['entries']==[] and saved['test']['folder']=='QA temporary'
    m.command('resetUnreadableLibrary','RESET UNREADABLE LIBRARY')
    after=m.json_get('LIBRARY');assert after['readable'] and after['entries']==[]
    print('Recovered only the failed test-library blob; empty pre-test user metadata restored. Pattern presets untouched.')
elif a.mode=='stage':
    saved=json.loads(a.file.read_text());before=saved['before'];lib=m.json_get('LIBRARY')
    assert len(state['catalog'])==56 and state['catalog'][:-1]==before['catalog']
    assert state['bookmarks'][:-1]==before['bookmarks'] and state['bookmarks'][-1] is False
    for k in ['targetFPS','autoCycleEnabled']:assert state[k]==before[k]
    test={'id':75,'rating':4,'folder':'QA temporary','family':'QA variants','tags':['test','geometry']}
    saved.setdefault('original',expanded(lib,75));saved['test']=test
    # Save restoration information before any mutation, even if test crashes.
    a.file.write_text(json.dumps(saved,indent=2)+'\n')
    code,result=post(test);assert code==200 and result['success'],result
    assert expanded(m.json_get('LIBRARY'),75)==test
    for fields in [{'rating':6},{'rating':True},{'id':-1},{'id':76},{'folder':'x'*25},{'tags':['a','b','c','d','e']},{'tags':['x'*21]}]:
        code,result=post(dict(test,**fields));assert code==400 and not result['success'],(fields,result)
        assert expanded(m.json_get('LIBRARY'),75)==test
    saved['staged']=True;a.file.write_text(json.dumps(saved,indent=2)+'\n');print('PASS API validation, saved metadata, append-only catalog and unchanged bookmarks/playback. Reboot then verify.')
else:
    saved=json.loads(a.file.read_text());assert saved['staged']
    assert expanded(m.json_get('LIBRARY'),75)==saved['test'],'Metadata lost across reboot'
    code,result=post(saved['original']);assert code==200 and result['success'],result
    assert expanded(m.json_get('LIBRARY'),75)==saved['original']
    before=saved['before'];m.command('setPatternVariant' if before['currentPattern']==16 else 'setPattern',before['params']['variant'] if before['currentPattern']==16 else before['currentPattern'])
    for key,value in before['params'].items():
        if key!='variant':m.command('setParam',{'animIndex':before['currentPattern'],'paramName':key,'value':value})
    after=m.json_get('CONTROL');assert after['params']==before['params']
    assert after['bookmarks'][:-1]==before['bookmarks']
    summary={'passed':['append-only catalog','bookmarks preserved','metadata validation','flash save','metadata survives firmware update/reboot','test record restored','playback parameters restored'],'diagnostics':{k:v for k,v in m.json_get('DIAGNOSTICS').items() if k in ['freeHeap','minimumFreeHeap','settingsFreeEntries','panelRefreshHz','colorDepth','displayClockMHz']}}
    (Path(__file__).resolve().parent.parent/'docs/library-organizer-verification.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
