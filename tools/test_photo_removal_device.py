"""Snapshot/verify non-secret network and bookmark fingerprints around OTA."""
import argparse,hashlib,json,urllib.request
from pathlib import Path
parser=argparse.ArgumentParser();parser.add_argument('mode',choices=('before','after'))
parser.add_argument('--host',required=True);parser.add_argument('--snapshot',type=Path,required=True)
args=parser.parse_args()
def get(path):
 with urllib.request.urlopen('http://'+args.host+'/'+path,timeout=12) as r:return json.load(r)
state=get('control');diag=get('diagnostics')
assert state['deviceProfile']=='esp32-wroom-ws2812-16x16'
networks=[{k:n.get(k) for k in ('index','ssid','preferred')} for n in state['savedNetworks']]
fingerprint=hashlib.sha256(json.dumps(networks,sort_keys=True).encode()).hexdigest()
catalog=state['catalog']
def key(p):return str(p['engine'])+':'+str(p.get('variant'))
bookmarks={key(p):bool(state['bookmarks'][i]) for i,p in enumerate(catalog) if not(p['engine']==16 and p.get('variant') in (56,57))}
if args.mode=='before':
 args.snapshot.write_text(json.dumps({'networks':fingerprint,'count':len(networks),'bookmarks':bookmarks}))
 print('Saved network/bookmark fingerprints; no passwords or SSIDs recorded')
else:
 old=json.loads(args.snapshot.read_text())
 assert old['networks']==fingerprint and old['count']==len(networks),'Saved networks changed'
 assert old['bookmarks']==bookmarks,'Unrelated bookmarks changed'
 assert len(catalog)==59 and not any(p['engine']==16 and p.get('variant') in (56,57) for p in catalog)
 assert diag['otaReady'] and state['wifiConnected']
 print('PASS: 59 patterns; both photos absent; saved networks and unrelated bookmarks unchanged; Wi-Fi/OTA ready')
