"""Exercise new calibration endpoints, restore playback, leave the gray chart.

Does not save test gains, alter pattern defaults or overwrite bookmarks.
"""
import json
import time
import urllib.error
from pathlib import Path
from profile_pattern_library import MatrixHttp
m=MatrixHttp('192.168.178.115')
before=json.loads(Path('/private/tmp/matrix-pre-calibration-state.json').read_text())
state=m.json_get('CONTROL')
assert len(state['catalog'])==len(before['catalog'])==55
assert state['bookmarks']==before['bookmarks']
assert state['targetFPS']==before['targetFPS']
assert state['autoCycleEnabled']==before['autoCycleEnabled']
assert state['calibration']['bits']==4 and state['depthComparisonAvailable']
baseline={k:state['calibration'][k] for k in ['r','g','b','gamma100','photoCorrection']}
try:
    for mode in [1,2,3,0]:
        m.command('setCalibrationMode',mode)
        assert m.json_get('CONTROL')['calibration']['mode']==mode
    for photo in [0,1]:
        m.command('setCalibrationPhoto',photo)
        assert m.json_get('CONTROL')['calibration']['photo']==photo
    for level in [32,128,96]:
        m.command('setCalibrationLevel',level)
        assert m.json_get('CONTROL')['calibration']['level']==level
    for correction in [False,True]:
        value=dict(r=90,g=85,b=80,gamma100=200,photoCorrection=correction)
        m.command('setColorCalibration',value)
        now=m.json_get('CONTROL')['calibration']
        assert all(now[k]==v for k,v in value.items())
    previous=m.json_get('CONTROL')['calibration']
    for command,value in [('setCalibrationMode',4),('setCalibrationPhoto',2),('setCalibrationLevel',255),
                          ('setColorCalibration',dict(baseline,gamma100=0)),
                          ('setColorCalibration',dict(baseline,r=101)),
                          ('setColorCalibration',{}),('setCalibrationMode',True)]:
        try:m.command(command,value)
        except (RuntimeError,urllib.error.HTTPError):pass
        else:raise AssertionError(f'Accepted invalid {command}: {value}')
    assert m.json_get('CONTROL')['calibration']==previous
    print('PASS: modes, both photos, level bounds, RGB balance, photo gamma toggle, invalid-input rejection')
finally:
    m.command('setColorCalibration',baseline)
    m.command('setCalibrationMode',0)
    m.command('setPatternVariant' if before['currentPattern']==16 else 'setPattern',
              before['params']['variant'] if before['currentPattern']==16 else before['currentPattern'])
    for key,value in before['params'].items():
        if key!='variant':m.command('setParam',dict(animIndex=before['currentPattern'],paramName=key,value=value))
    m.command('setCalibrationPhoto',0)
    m.command('setCalibrationLevel',96)
    now=m.json_get('CONTROL')
    assert now['params']==before['params']
    assert now['bookmarks']==before['bookmarks']
    m.command('setCalibrationMode',1)
    time.sleep(2)
    d=m.json_get('DIAGNOSTICS')
    print(f"Gray/RGB chart active. Actual depth: {d['colorDepth']} bits; electrical refresh: {d['panelRefreshHz']:.1f} Hz.")
    print('PASS: prior pattern parameters, bookmarks and playback settings preserved; test gains restored')
