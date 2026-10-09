#!/usr/bin/env python3
"""Reversible live API tests. --prepare leaves a temporary saved test default;
--restore verifies it after reboot, restores original values and playback.
The snapshot contains only pattern/playback settings, never Wi-Fi credentials.
"""
import argparse,json,pathlib,urllib.request
ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('--host',required=True)
ap.add_argument('--prepare',action='store_true')
ap.add_argument('--restore',action='store_true')
ap.add_argument('--snapshot',default='/private/tmp/matrix-library-test.json')
args=ap.parse_args();base='http://'+args.host
def get():
    return json.load(urllib.request.urlopen(base+'/control',timeout=20))
def cmd(name,**fields):
    req=urllib.request.Request(base+'/control',json.dumps({'command':name,**fields}).encode(),{'Content-Type':'application/json'})
    d=json.load(urllib.request.urlopen(req,timeout=20))
    assert d.get('success'),d.get('message',d)
def param(k,v):cmd('setParam',animIndex=16,paramName=k,value=v)
snapshot=pathlib.Path(args.snapshot)
if args.prepare:
    initial=get();assert len(initial['catalog'])==46 and not any(initial['bookmarks'])
    saved={'engine':initial['currentPattern'],'params':initial['params'],'fps':initial['targetFPS'],'auto':initial['autoCycleEnabled']}
    cmd('setAutoCycle',value=False);cmd('setPatternVariant',value=17)
    original=get();saved['original']=original['params'];snapshot.write_text(json.dumps(saved))
    assert original['params']['palette']==7
    for k,v in {'c0':31,'palette':8,'color0':'#112233','lfo_target':1,'lfo_speed':.8,'lfo_amount':1,'rotation':5}.items():param(k,v)
    cmd('saveCurrentDefault');cmd('setPatternVariant',value=18)
    other=get()['params'];assert other['palette']==7 and other['rotation']==0 and other['lfo_target']==0
    cmd('setPatternVariant',value=17);p=get()['params']
    assert p['c0']==31 and p['palette']==8 and p['color0'].lower()=='#112233' and p['rotation']==5 and p['lfo_target']==1
    cmd('setDefault',animIndex=16,patchIndex=1);cmd('loadPatch',animIndex=16,patchIndex=1)
    assert get()['params']['c0']==30 # selecting an auto slot must not save over it
    cmd('setDefault',animIndex=16,patchIndex=0);cmd('loadPatch',animIndex=16,patchIndex=0)
    assert get()['params']['c0']==31
    cmd('setPattern',value=0)
    print('PASS: 46 entries, cleared bookmarks, Original default, independent palettes/LFO/style and non-overwriting auto slot. Prepared reboot test.',flush=True)
elif args.restore:
    saved=json.loads(snapshot.read_text());cmd('setPatternVariant',value=17);p=get()['params']
    assert p['c0']==31 and p['palette']==8 and p['color0'].lower()=='#112233' and p['rotation']==5 and p['lfo_target']==1
    cmd('setDefault',animIndex=16,patchIndex=1);cmd('loadPatch',animIndex=16,patchIndex=1)
    assert get()['params']['c0']==30
    cmd('setDefault',animIndex=16,patchIndex=0);cmd('loadPatch',animIndex=16,patchIndex=0)
    assert get()['params']['c0']==31
    param('color0','#160A20') # restore the temporary Ember-bank edit too
    for k,v in saved['original'].items():
        if k!='variant':param(k,v)
    cmd('saveCurrentDefault');cmd('setPatternVariant',value=18);cmd('setPatternVariant',value=17)
    p=get()['params'];assert p['c0']==30 and p['palette']==7 and p['rotation']==0
    if saved['engine']==16:cmd('setPatternVariant',value=saved['params']['variant'])
    else:cmd('setPattern',value=saved['engine'])
    for k,v in saved['params'].items():
        if k!='variant':cmd('setParam',animIndex=saved['engine'],paramName=k,value=v)
    cmd('setFPS',value=saved['fps']);cmd('setAutoCycle',value=saved['auto'])
    assert not any(get()['bookmarks'])
    print('PASS: saved per-pattern state survived reboot; all temporary test edits restored.',flush=True)
else:ap.error('Choose --prepare or --restore')
