"""Undo only audited mounting-angle compensations after global output rotation.

Uses the pre-change snapshot as a safety check; never guesses arbitrary angles.
Optional --collatz-zero applies the user's explicit 100 -> 0 degree correction.
"""
import argparse
import json
import urllib.request
from pathlib import Path
from profile_pattern_library import MatrixHttp

p=argparse.ArgumentParser();p.add_argument('--collatz-zero',action='store_true');a=p.parse_args()
m=MatrixHttp('192.168.178.115');before=m.json_get('CONTROL')
assert before['outputRotationDegrees']==90 and not before['autoCycleEnabled']
snapshot=json.loads(Path('docs/pattern-rotation-before.json').read_text())
changes=[]
report=Path('docs/pattern-rotation-corrections.json')
prior=json.loads(report.read_text()) if report.exists() else []
def post(command,**fields):
    req=urllib.request.Request(m.base+'/control',json.dumps({'command':command,**fields}).encode(),{'Content-Type':'application/json'})
    with urllib.request.urlopen(req,timeout=10) as r:d=json.load(r)
    assert d['success'],d
try:
    for item in snapshot['patterns']:
        for slot,original in enumerate(item['slots']):
            angle=original.get('rotation',0)
            if angle!=90 and not (a.collatz_zero and item['name']=='Collatz Kelp' and angle==100):continue
            target=0
            m.command('setPatternVariant' if item['variant'] is not None else 'setPattern',item['variant'] if item['variant'] is not None else item['engine'])
            post('loadPatch',animIndex=item['engine'],patchIndex=slot)
            now=m.json_get('CONTROL')['params']
            if now==dict(original,rotation=target):continue # Already corrected.
            assert now==original,('Preset changed since audit; refusing overwrite',item['name'],slot)
            m.command('setParam',dict(animIndex=item['engine'],paramName='rotation',value=target))
            post('savePatch',animIndex=item['engine'],patchIndex=slot)
            post('loadPatch',animIndex=item['engine'],patchIndex=slot)
            assert m.json_get('CONTROL')['params']==dict(original,rotation=target)
            change={'name':item['name'],'engine':item['engine'],'variant':item['variant'],'slot':slot,'from':angle,'to':target}
            changes.append(change);print(json.dumps(change),flush=True)
            report.write_text(json.dumps(prior+changes,indent=2)+'\n')
finally:
    m.command('setPatternVariant' if before['currentPattern']==16 else 'setPattern',
              before['params']['variant'] if before['currentPattern']==16 else before['currentPattern'])
    restored=dict(before['params'])
    if restored.get('rotation')==90:restored['rotation']=0
    if a.collatz_zero and any(c['variant']==restored.get('variant') and c['name']=='Collatz Kelp' for c in changes) and restored.get('rotation')==100:
        restored['rotation']=0
    for key,value in restored.items():
        if key!='variant':m.command('setParam',dict(animIndex=before['currentPattern'],paramName=key,value=value))
    if before['calibration']['mode']:m.command('setCalibrationMode',before['calibration']['mode'])
    now=m.json_get('CONTROL')
    assert now['params']==restored and now['bookmarks']==before['bookmarks']
    assert now['targetFPS']==before['targetFPS']
    print('Selection restored; only audited rotation values changed',flush=True)
