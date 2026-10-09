"""Read-only state checks plus deliberately invalid commands; no Wi-Fi writes."""
import json,time,urllib.request,urllib.error
base='http://192.168.178.115'
for attempt in range(30):
 try:
  with urllib.request.urlopen(base+'/control',timeout=2) as r:state=json.load(r)
  break
 except Exception:
  if attempt==29:raise
  time.sleep(1)
networks=state['savedNetworks'];assert len(networks)<=8
assert len({n['index'] for n in networks})==len(networks)
assert all(set(n)=={'index','ssid','preferred','connected'} for n in networks)
assert all(isinstance(n['ssid'],str) and 0<=n['index']<8 for n in networks)
for body in ({'command':'connectWifi','value':-1},{'command':'connectWifi','value':8},{'command':'saveWifiNetwork','value':{'ssid':'','password':''}}):
 request=urllib.request.Request(base+'/control',json.dumps(body).encode(),{'Content-Type':'application/json'})
 try:urllib.request.urlopen(request,timeout=5)
 except urllib.error.HTTPError as e:assert e.code==400 and json.load(e)['success'] is False
 else:raise AssertionError('Invalid Wi-Fi request accepted')
with urllib.request.urlopen(base+'/control',timeout=5) as r:after=json.load(r)
assert after['savedNetworks']==networks,'Invalid command mutated Wi-Fi state'
with urllib.request.urlopen(base+'/',timeout=10) as r:html=r.read().decode()
for marker in ('id="savedWifi"','Connect selected','id="wifiMessage"','restart:false','No confirmation received'):
 assert marker in html,marker
print(f'PASS live firmware: {len(networks)} saved networks, no passwords exposed, invalid requests rejected without changes, new dashboard served')
