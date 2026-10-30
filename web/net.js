// Requests, the one-at-a-time write queue and state refreshes shared by every view.
const $=id=>document.getElementById(id);
let state=null,timers={},lastStateRevision=null;
let commandQueue=Promise.resolve(),pendingCommands=0,writeVersion=0,refreshPromise=null,refreshRequested=false,diagnosticsBusy=false;
let refreshControls=false,pointerEditing=false;

function element(tag,className,text){const node=document.createElement(tag);node.className=className;if(text!==undefined)node.textContent=text;return node}
function toast(message,ok=true){const t=$('toast');t.textContent=message;t.className='toast show'+(ok?'':' bad');clearTimeout(toast._t);toast._t=setTimeout(()=>t.className='toast',ok?2600:6000)}
// Consequential buttons ask on themselves: the first press arms the button for 4 s without letting it narrow,
// and a later press confirms once 400 ms have passed, asking again if what it acts on has changed meanwhile.
function confirmOnButton(button,question,subject){
 const disarm=()=>{clearTimeout(button._disarm);button.textContent=button.dataset.label;button.style.minWidth='';delete button.dataset.armed};
 if(button.dataset.armed){
  if(Date.now()-button._armedAt<400)return false;
  if(button._subject===subject){disarm();return true}
  clearTimeout(button._disarm);
 }else{button.dataset.label=button.textContent;button.style.minWidth=button.offsetWidth+'px';button.dataset.armed='true';button.textContent=question}
 button._subject=subject;button._armedAt=Date.now();button._disarm=setTimeout(disarm,4000);return false;
}
// The badge appears only when something needs attention.
function connectionStatus(connected){
 const note=!connected?'Offline':state?.networkMode==='ap'?'Setup network':state?.wifiConnected===false?'Wi-Fi disconnected':'';
 document.body.dataset.connected=String(connected);$('connectionNote').textContent=note;$('connection').hidden=!note;
}
async function api(url,opt){
 const controller=new AbortController(),timeout=setTimeout(()=>controller.abort(),12000);
 try{const r=await fetch(url,{...opt,signal:controller.signal});let d;
  try{d=await r.json()}catch(e){throw new Error('The matrix sent no confirmation.')}
  if(!r.ok||!d||d.success!==true)throw new Error(d?.message||('Request failed (HTTP '+r.status+')'));
  return d;
 }catch(e){if(e.name==='AbortError')throw new Error('The matrix did not answer in time.');if(e.name==='TypeError')throw new Error('The matrix did not answer.');throw e}
 finally{clearTimeout(timeout)}
}
// Writes run one at a time; a poll that overlaps one is discarded and repeated.
function queueWrite(operation){
 pendingCommands++;writeVersion++;
 const run=commandQueue.then(operation);commandQueue=run.catch(()=>{});
 return run.finally(()=>{pendingCommands--;refreshWhenIdle()});
}
function postJson(url,body){return api(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})}
// Pattern writes capture the playing pattern when issued, so a later switch makes them fail instead of editing another pattern.
function post(body){
 const scoped=['setParam','resetPattern'];
 if(state&&scoped.includes(body.command)&&body.expectedPattern===undefined)body={...body,expectedPattern:state.currentPattern,expectedVariant:state.params.variant||0};
 return queueWrite(()=>postJson('/control',body));
}
async function command(command,extra={}){
 if(command!=='setParam')flushEdits();
 try{await post({command,...extra});if(['setPattern','setPatternVariant','resetPattern','setAutoCycle'].includes(command))await refreshState(true);return true}
 catch(e){toast(e.message,false);await refreshState(true);return false}
}
// Send the latest value after a short pause, and at least every 300 ms while dragging.
function debounce(key,fn){
 const since=timers[key]?timers[key].since:Date.now();if(timers[key])clearTimeout(timers[key].timer);
 if(Date.now()-since>=300){delete timers[key];fn();return}
 timers[key]={fn,since,timer:setTimeout(()=>{delete timers[key];fn()},130)};
}
function flushEdits(){const queued=Object.values(timers);timers={};for(const edit of queued){clearTimeout(edit.timer);edit.fn()}}
function refreshWhenIdle(){if(refreshRequested&&!pendingCommands&&!refreshPromise){refreshRequested=false;refreshState()}}
function refreshState(forceControls=false){
 refreshControls=refreshControls||forceControls;
 if(refreshPromise)return refreshPromise;
 if(pendingCommands){refreshRequested=true;return Promise.resolve(false)}
 refreshPromise=(async()=>{try{
  const version=writeVersion,next=await api('/control');
  if(pendingCommands||version!==writeVersion){refreshRequested=true;return false;}
  state=next;lastStateRevision=state.stateRevision;
  loadPreviewAtlas(state.previewAtlasKey||'default');
  render(refreshControls);refreshControls=false;renderSavedNetworks();return true;
 }catch(e){lastStateRevision=null;connectionStatus(false);toast(e.message,false);return false}finally{refreshPromise=null;refreshWhenIdle()}})();
 return refreshPromise;
}
// Diagnostics double as the heartbeat: a changed revision, or recovery after a failure, triggers a state read.
async function refreshDiagnostics(){
 if(diagnosticsBusy)return;diagnosticsBusy=true;
 try{
  const d=await api('/diagnostics');
  connectionStatus(true);if(!state||lastStateRevision!==d.stateRevision)await refreshState();
 }catch(e){lastStateRevision=null;connectionStatus(false)}
 finally{diagnosticsBusy=false}
}
