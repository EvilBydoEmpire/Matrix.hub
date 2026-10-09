#!/usr/bin/env python3
"""Compare native and interpolated quality, restoring device settings afterward."""
import argparse
import json
import pathlib
import time
from profile_pattern_library import MatrixHttp

parser=argparse.ArgumentParser()
parser.add_argument('--host',required=True)
args=parser.parse_args()
device=MatrixHttp(args.host)
initial=device.json_get('CONTROL')
results=[]
try:
    device.command('setAutoCycle',False)
    device.command('setFPS',240)
    for variant in (1,7,20,34):
        device.command('setPatternVariant',variant)
        quality=device.json_get('CONTROL')['patternQuality']
        try:
            for mode in (0,1):
                device.command('setPatternQuality',mode)
                time.sleep(3)
                d=device.json_get('DIAGNOSTICS')
                assert d['renderWidth']==(64 if mode==0 else (24 if variant in (7,20) else 32))
                results.append({'variant':variant,'mode':'native' if mode==0 else 'smooth fast',
                    'pattern':d['pattern'],'actualFPS':d['actualFPS'],
                    'renderWidth':d['renderWidth'],'renderHeight':d['renderHeight'],
                    'averageRenderUs':d['averageRenderUs']})
                print(f"{d['pattern']}: {results[-1]['mode']} {d['renderWidth']}×{d['renderHeight']} {d['actualFPS']:.1f} FPS",flush=True)
        finally:
            device.command('setPatternQuality',quality)
finally:
    device.command('setFPS',initial['targetFPS'])
    if initial['currentPattern']==16:
        device.command('setPatternVariant',initial['params']['variant'])
    else:
        device.command('setPattern',initial['currentPattern'])
    for key,value in initial['params'].items():
        if key!='variant':
            device.command('setParam',{'animIndex':initial['currentPattern'],'paramName':key,'value':value})
    device.command('setAutoCycle',initial['autoCycleEnabled'])
root=pathlib.Path(__file__).resolve().parent.parent
(root/'docs/pattern-benchmark-quality-comparison.json').write_text(json.dumps(results,indent=2)+'\n')
