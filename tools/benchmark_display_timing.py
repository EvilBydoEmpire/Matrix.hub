"""Read-only scan-rate + dashboard responsiveness probe. Never changes patterns.

For DMA probe results, retain independent E-counter and driver-calculated
refresh fields: a calculated value is not a measured scan rate.
"""
import argparse
import json
import statistics
import time
import urllib.request
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('--host',default='192.168.178.115')
p.add_argument('--samples',type=int,default=40)
p.add_argument('--interval',type=float,default=.5)
p.add_argument('--report',type=Path,required=True)
a=p.parse_args()
if not 1<=a.samples<=1000 or not .1<=a.interval<=10:p.error('Invalid sample count/interval')
rows=[];failures=[]
for index in range(a.samples):
    start=time.monotonic()
    try:
        with urllib.request.urlopen('http://'+a.host+'/diagnostics',timeout=2) as r:d=json.load(r)
        row={'sample':index,'httpMs':round((time.monotonic()-start)*1000,2)}
        # Exclude Wi-Fi SSID, IP and other incidental device configuration.
        fields=('pattern','colorDepth','timingProfile','displayClockMHz','panelRefreshHz','actualFPS',
                'freeHeap','minimumFreeHeap','averageRenderUs','probe','running','clockMHz',
                'driverCalculatedRefreshHz','addressEScanHz','counterReady','renderFPS','rssi')
        row.update({k:d[k] for k in fields if k in d});rows.append(row)
    except Exception as e:failures.append({'sample':index,'error':type(e).__name__})
    time.sleep(max(0,a.interval-(time.monotonic()-start)))
latencies=sorted(row['httpMs'] for row in rows)
summary={'requests':a.samples,'successes':len(rows),'failures':len(failures)}
if rows:
    summary.update(httpMedianMs=statistics.median(latencies),httpP95Ms=latencies[min(len(latencies)-1,int(len(latencies)*.95))])
    for key in ('panelRefreshHz','addressEScanHz','driverCalculatedRefreshHz','actualFPS','renderFPS','freeHeap'):
        values=[r[key] for r in rows if key in r]
        if values:summary[key]={'min':min(values),'median':statistics.median(values),'max':max(values)}
a.report.write_text(json.dumps({'summary':summary,'samples':rows,'errors':failures},indent=2)+'\n')
print(json.dumps(summary,indent=2))
