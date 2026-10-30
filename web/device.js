// Device view: schedule, Wi-Fi, display tuning and identity.
let scheduleDirty=false,wifiBusy=false;
function minutesToTime(value){const m=Math.max(0,Math.min(1439,Number(value)||0));return String(Math.floor(m/60)).padStart(2,'0')+':'+String(m%60).padStart(2,'0')}
function timeToMinutes(value){const p=String(value||'00:00').split(':').map(Number);return Math.max(0,Math.min(1439,p[0]*60+p[1]))}
function renderSchedule(){
 if(scheduleDirty||$('scheduleFields').contains(document.activeElement))return;
 $('scheduleEnabled').checked=state.scheduleEnabled;$('scheduleFields').hidden=!state.scheduleEnabled;
 $('scheduleStart').value=minutesToTime(state.scheduleStartMinutes);$('scheduleEnd').value=minutesToTime(state.scheduleEndMinutes);
 $('scheduleFadeIn').value=state.scheduleFadeInSeconds;$('scheduleFadeOut').value=state.scheduleFadeOutSeconds;
}
// Edits save as one command once the shown fields are valid; a failed save keeps the draft until the next edit retries it.
function editSchedule(){
 const enabled=$('scheduleEnabled').checked;$('scheduleFields').hidden=!enabled;
 const invalid=[...$('scheduleFields').querySelectorAll('input')].find(field=>!field.checkValidity());
 if(enabled&&invalid){invalid.reportValidity();return}
 // The firmware takes every field at once, so a hidden field left invalid keeps its saved value.
 const read=(id,parse,saved)=>$(id).checkValidity()?parse($(id).value):saved;
 const body={command:'setSchedule',enabled,startMinutes:read('scheduleStart',timeToMinutes,state.scheduleStartMinutes),endMinutes:read('scheduleEnd',timeToMinutes,state.scheduleEndMinutes),
  fadeInSeconds:read('scheduleFadeIn',Number,state.scheduleFadeInSeconds),fadeOutSeconds:read('scheduleFadeOut',Number,state.scheduleFadeOutSeconds)};
 scheduleDirty=true;
 debounce('schedule',async()=>{try{await post(body);scheduleDirty=false}catch(e){toast(e.message,false)}refreshState()});
}
// The setup network has no .local names, so it shows the address alone.
function renderAbout(){
 const address=state.networkMode==='ap'?state.ip:`${state.hostname}.local · ${state.ip}`;
 const rows=[['Panel',`${state.displayWidth}×${state.displayHeight} ${state.backend==='ws2812'?'WS2812B':'HUB75'}`],['Address',address],['Firmware',`${state.projectName} by ${state.projectAuthor}`+(state.firmware?` · ${state.firmware}`:'')]];
 $('about').replaceChildren(...rows.flatMap(([term,value])=>[element('dt','',term),element('dd','',value)]));
}
function renderCalibration(){
 const c=state.calibration;$('calibrationPanel').hidden=!c;if(!c)return;
 $('testChart').checked=c.chart;
 for(const [id,value] of [['calR',c.r],['calG',c.g],['calB',c.b]]){const range=$(id);if(!timers.calibration&&!pointerEditing&&document.activeElement!==range)range.value=value;showBalance(range)}
}
function showBalance(range){paintRange(range);range.previousElementSibling.textContent=range.value+' %'}
function editBalance(range){showBalance(range);debounce('calibration',setColorCalibration)}
function setColorCalibration(){return command('setColorCalibration',{value:{r:Number($('calR').value),g:Number($('calG').value),b:Number($('calB').value)}})}
function resetColorCalibration(button){
 if(!confirmOnButton(button,'Confirm reset'))return;
 const d=state.calibration?.default||{r:100,g:100,b:100};
 for(const [id,value] of [['calR',d.r],['calG',d.g],['calB',d.b]]){$(id).value=value;showBalance($(id))}
 return setColorCalibration()
}
// Wi-Fi changes report only what the matrix confirmed and keep their inputs until then.
function wifiMessage(text,ok=true){const message=$('wifiMessage');message.hidden=!text;message.textContent=text;message.style.color=ok?'':'var(--bad)'}
function savedNetwork(){return (state.savedNetworks||[]).find(n=>String(n.index)===$('savedWifi').value)}
// The picker lists the saved networks the matrix could move to.
function renderSavedNetworks(){
 const select=$('savedWifi'),others=(state.savedNetworks||[]).filter(n=>!n.connected),old=select.value;
 if(document.activeElement!==select){
  select.replaceChildren(...others.map(n=>{const option=element('option','',n.ssid);option.value=String(n.index);return option}));
  if(others.some(n=>String(n.index)===old))select.value=old;else if(others.length)select.value=String((others.find(n=>n.preferred)||others[0]).index);
 }
 $('wifiPick').hidden=!others.length;select.disabled=$('connectWifi').disabled=wifiBusy;
 for(const id of ['saveWifi','forgetWifi','newSsid','newPass'])$(id).disabled=wifiBusy;
 $('networkInfo').textContent=state.networkMode==='ap'?`Setup network “${state.apSsid}” at http://${state.ip}/`:state.wifiConnected===false?'Wi-Fi disconnected':`Connected to ${state.ssid||'Wi-Fi'}`+(state.rssi?` · ${String(state.rssi).replace('-','−')} dBm`:'');
}
function wifiRequest(url,body){flushEdits();return queueWrite(()=>postJson(url,body))}
// Controls unlock once a restart has had time to finish, whether or not the network returns.
function afterRestart(){setTimeout(()=>{wifiBusy=false;renderSavedNetworks();refreshState()},12000)}
async function addNetwork(){
 if(wifiBusy)return;const ssid=$('newSsid').value.trim(),pass=$('newPass').value;
 if(!ssid){wifiMessage('Enter a network name.',false);return}
 wifiBusy=true;renderSavedNetworks();wifiMessage('Saving…');
 try{
  await wifiRequest('/wifi/add',{ssid,password:pass,restart:false});
  $('newSsid').value='';$('newPass').value='';wifiMessage('Saved “'+ssid+'”.');
  await refreshState();const n=(state.savedNetworks||[]).find(n=>n.ssid===ssid);if(n)$('savedWifi').value=String(n.index);
 }catch(e){wifiMessage('Not saved: '+e.message,false);await refreshState()}
 finally{wifiBusy=false;renderSavedNetworks()}
}
async function connectSavedNetwork(button){
 if(wifiBusy)return;const n=savedNetwork();
 if(!n){wifiMessage('Choose a saved network first.',false);return}
 if(!confirmOnButton(button,'Confirm switch',n.index))return;
 wifiBusy=true;renderSavedNetworks();wifiMessage('Switching to “'+n.ssid+'”…');
 try{await wifiRequest('/control',{command:'connectWifi',value:n.index});wifiMessage('Restarting on “'+n.ssid+'”. Join that network and reopen http://'+state.hostname+'.local/.');afterRestart()}
 catch(e){wifiBusy=false;renderSavedNetworks();wifiMessage('Switch not confirmed: '+e.message,false)}
}
async function resetWifi(button){
 if(wifiBusy||!confirmOnButton(button,'Confirm forget all'))return;
 wifiBusy=true;renderSavedNetworks();wifiMessage('Forgetting all networks…');
 try{await wifiRequest('/wifi/reset',{});wifiMessage(`Networks forgotten. The matrix restarts into its setup network “${state.apSsid}”; join it to add a network.`);afterRestart()}
 catch(e){wifiBusy=false;renderSavedNetworks();wifiMessage('Not forgotten: '+e.message,false);await refreshState()}
}
