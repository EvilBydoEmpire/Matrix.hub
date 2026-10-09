"""Two temporary saved-color probes across OTA; restore all edited colors.
Run prepare, reboot via the normal firmware update, then finish.
No Wi-Fi credentials or unrelated device state are recorded.
"""
import argparse,json,time,urllib.request
from pathlib import Path
from profile_pattern_library import MatrixHttp
p=argparse.ArgumentParser();p.add_argument('mode',choices=['prepare','finish']);a=p.parse_args()
m=MatrixHttp('192.168.178.115');file=Path('/private/tmp/soukan-persistence.json')
# OTA completion precedes Wi-Fi readiness; do not start the mutation/restore
# transaction while ARP and the HTTP listener are still coming back online.
for attempt in range(30):
 try:
  with urllib.request.urlopen(m.base+'/control',timeout=2) as response:json.load(response)
  break
 except Exception:
  if attempt==29:raise
  time.sleep(1)
def apply(params,engine=16):
 for k,v in params.items():
  if k!='variant':m.command('setParam',{'animIndex':engine,'paramName':k,'value':v})
def select(s):
 m.command('setPatternVariant' if s['currentPattern']==16 else 'setPattern',s['params']['variant'] if s['currentPattern']==16 else s['currentPattern'])
if a.mode=='prepare':
 state=m.json_get('CONTROL')
 data={'initial':{k:state[k] for k in ('currentPattern','params','autoCycleEnabled','targetFPS','bookmarks')},'probes':[]}
 file.write_text(json.dumps(data,indent=2)+'\n')
 m.command('setAutoCycle',False)
 for variant,palette in ((61,14),(62,7)):
  m.command('setPatternVariant',variant);original=m.json_get('CONTROL')['params']
  apply({'palette':palette});oldStop=m.json_get('CONTROL')['params']['color1']
  row={'variant':variant,'original':original,'testPalette':palette,'oldStop':oldStop}
  data['probes'].append(row);file.write_text(json.dumps(data,indent=2)+'\n')
  apply({'color1':'#C49A73'});m.command('saveCurrentDefault',0)
  row['expected']=m.json_get('CONTROL')['params'];file.write_text(json.dumps(data,indent=2)+'\n')
 print('Prepared editable Soukan and legacy Original defaults for reboot verification.')
else:
 data=json.loads(file.read_text());verified=[]
 try:
  m.command('setAutoCycle',False)
  assert m.json_get('CONTROL')['bookmarks']==data['initial']['bookmarks']
  for row in data['probes']:
   m.command('setPatternVariant',row['variant'])
   assert m.json_get('CONTROL')['params']==row['expected'],row['variant']
   verified.append(row['variant'])
 finally:
  for row in data['probes']:
   m.command('setPatternVariant',row['variant'])
   apply({'palette':row['testPalette'],'color1':row['oldStop']})
   apply(row['original']);m.command('saveCurrentDefault',0)
   # Switching away/back re-reads the restored default, not the live controls.
   m.command('setPattern',0);m.command('setPatternVariant',row['variant'])
   assert m.json_get('CONTROL')['params']==row['original']
  select(data['initial']);apply(data['initial']['params'],data['initial']['currentPattern'])
  m.command('setAutoCycle',data['initial']['autoCycleEnabled'])
 result={'verifiedAcrossReboot':verified,'editableSoukanAndLegacyPalette':len(verified)==2,'restoredAllProbeColors':True}
 (Path(__file__).resolve().parent.parent/'docs/soukan-persistence.json').write_text(json.dumps(result,indent=2)+'\n')
 print('PASS',result)
