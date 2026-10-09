"""Snapshot all active preset slots without saving changes; restore selection."""
import json
from pathlib import Path
import urllib.request
from profile_pattern_library import MatrixHttp

m=MatrixHttp('192.168.178.115')
before=m.json_get('CONTROL')
assert not before['autoCycleEnabled'],'Pause auto-cycle before auditing'
snapshot={'before':{k:before[k] for k in ('currentPattern','params','bookmarks','targetFPS','autoCycleEnabled','calibration')},'patterns':[]}
report=Path('docs/pattern-rotation-before.json')
def post(command,**fields):
    req=urllib.request.Request(m.base+'/control',json.dumps({'command':command,**fields}).encode(),{'Content-Type':'application/json'})
    with urllib.request.urlopen(req,timeout=8) as r: result=json.load(r)
    assert result['success'],result
try:
    for item in before['catalog']:
        m.command('setPatternVariant' if item['variant'] is not None else 'setPattern',item['variant'] if item['variant'] is not None else item['engine'])
        default=m.json_get('CONTROL')['defaultPatches'][item['engine']]
        row={**item,'defaultSlot':default,'slots':[]}
        for slot in range(3):
            post('loadPatch',animIndex=item['engine'],patchIndex=slot)
            params=m.json_get('CONTROL')['params'];row['slots'].append(params)
            if params.get('rotation',0):print(item['name'],'slot',slot,'rotation',params['rotation'],flush=True)
        snapshot['patterns'].append(row)
        report.write_text(json.dumps(snapshot,indent=2)+'\n')
finally:
    m.command('setPatternVariant' if before['currentPattern']==16 else 'setPattern',
              before['params']['variant'] if before['currentPattern']==16 else before['currentPattern'])
    for key,value in before['params'].items():
        if key!='variant':m.command('setParam',dict(animIndex=before['currentPattern'],paramName=key,value=value))
    if before['calibration']['mode']:m.command('setCalibrationMode',before['calibration']['mode'])
    now=m.json_get('CONTROL')
    assert now['params']==before['params'] and now['bookmarks']==before['bookmarks']
    print('Restored selection; presets unchanged',flush=True)
