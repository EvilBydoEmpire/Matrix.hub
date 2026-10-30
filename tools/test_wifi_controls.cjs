const vm=require('node:vm'),assert=require('node:assert/strict'),web=require('./web_source.cjs');
const els={};function el(id){return els[id]||=(id==='savedWifi'?{value:'',options:[],replaceChildren(...options){this.options=options;this.value=options[0]?.value||''}}:{value:'',textContent:'',offsetWidth:96,style:{},dataset:{},closest(){return {}}})}
const calls=[],scheduled=new Map();let clock=0,timerId=0,refreshSucceeds=true;let refreshes=0;let response={success:true};let fail=null;
const state={networkMode:'station',ssid:'Home',rssi:-59,wifiConnected:true,hostname:'matrix-64',apSsid:'Matrix.64-Setup',ip:'192.168.1.20',displayWidth:64,displayHeight:64,backend:'hub75-dma',projectName:'Matrix.hub',projectAuthor:'Televator',firmware:'0e02abc',savedNetworks:[{index:0,ssid:'Home',connected:true},{index:1,ssid:'Studio <test>',preferred:true},{index:2,ssid:'Garden'}]};
const ctx=vm.createContext({$:el,state,Date:{now:()=>clock},flushEdits(){},queueWrite:operation=>operation(),document:{createElement(){return{value:'',textContent:''}},activeElement:null},AbortController,
 setTimeout(callback){scheduled.set(++timerId,callback);return timerId},clearTimeout(id){scheduled.delete(id)},toast(){},async refreshState(){refreshes++;if(refreshSucceeds)ctx.renderSavedNetworks();return refreshSucceeds},async fetch(url,opts){calls.push({url,body:JSON.parse(opts.body)});if(fail)throw fail;return{ok:response.success!==false,async json(){if(response==='invalid')throw Error('invalid');return response}}}});
vm.runInContext(web.pick('net.js','element','confirmOnButton','api','postJson')+'\n'+web.pick('device.js','scheduleDirty','renderAbout','wifiMessage','savedNetwork','renderSavedNetworks','wifiRequest','afterRestart','addNetwork','connectSavedNetwork','resetWifi'),ctx);
function expireTimers(){for(const [id,callback] of [...scheduled]){scheduled.delete(id);callback()}}
(async()=>{
 // About names the firmware's commit, and the setup network's address without a .local name it cannot resolve.
 const about=()=>{const rows=[];el('about').replaceChildren=(...nodes)=>rows.push(...nodes.map(n=>n.textContent));ctx.renderAbout();return rows};
 assert.deepEqual(about(),['Panel','64×64 HUB75','Address','matrix-64.local · 192.168.1.20','Firmware','Matrix.hub by Televator · 0e02abc']);
 Object.assign(state,{networkMode:'ap',ip:'192.168.4.1',firmware:''});
 assert.deepEqual(about().slice(3),['192.168.4.1','Firmware','Matrix.hub by Televator']);
 Object.assign(state,{networkMode:'station',ip:'192.168.1.20'});
 ctx.renderSavedNetworks();assert.deepEqual(el('savedWifi').options.map(o=>o.textContent),['Studio <test>','Garden'],'The picker lists the networks the matrix could move to');
 assert.equal(el('savedWifi').value,'1','The preferred network comes preselected');assert.equal(el('networkInfo').textContent,'Connected to Home · −59 dBm');
 el('savedWifi').value='2';ctx.renderSavedNetworks();assert.equal(el('savedWifi').value,'2','Refresh must preserve selection');
 await ctx.addNetwork();assert.equal(calls.length,0);assert.match(el('wifiMessage').textContent,/Enter/);
 el('newSsid').value='Studio <test>';el('newPass').value='test-only';
 const normalFetch=ctx.fetch;let finishSave;
 ctx.fetch=(...args)=>new Promise(resolve=>finishSave=()=>resolve(normalFetch(...args)));
 const saving=ctx.addNetwork();assert.equal(el('newSsid').disabled,true);assert.equal(el('newPass').disabled,true);assert.equal(el('forgetWifi').disabled,true);
 finishSave();await saving;ctx.fetch=normalFetch;
 assert.equal(el('newSsid').disabled,false);assert.equal(el('newPass').disabled,false);assert.equal(el('forgetWifi').disabled,false);
 assert.equal(calls.at(-1).body.restart,false);assert.equal(el('newPass').value,'');assert.match(el('wifiMessage').textContent,/Saved/);assert.equal(el('savedWifi').value,'1','The saved network is ready to connect');
 el('newSsid').value='Unsaved';el('newPass').value='retain-on-error';response={success:false,message:'Storage full'};
 const previousRefreshes=refreshes;await ctx.addNetwork();assert.equal(refreshes,previousRefreshes+1,'A failed save must refresh possibly changed network storage');assert.match(el('wifiMessage').textContent,/Storage full/);assert.equal(el('newPass').value,'retain-on-error');assert.equal(el('saveWifi').disabled,false);assert.equal(el('forgetWifi').disabled,false);
 response='invalid';await ctx.addNetwork();assert.match(el('wifiMessage').textContent,/sent no confirmation/);assert.equal(el('newSsid').value,'Unsaved');
 fail=Object.assign(new Error(),{name:'AbortError'});await ctx.addNetwork();assert.match(el('wifiMessage').textContent,/did not answer/);fail=null;
 // Connect asks on its button, keeps its width while asking, and confirms only a deliberate second press for the same network.
 response={success:true};el('savedWifi').value='1';const asked=calls.length,connect=Object.assign(el('connectWifi'),{textContent:'Connect'});
 await ctx.connectSavedNetwork(connect);assert.equal(calls.length,asked,'The first press only asks');assert.equal(connect.textContent,'Confirm switch');assert.equal(connect.style.minWidth,'96px');
 clock+=100;await ctx.connectSavedNetwork(connect);assert.equal(calls.length,asked,'A double click or key repeat only asks');
 el('savedWifi').value='2';clock+=400;await ctx.connectSavedNetwork(connect);assert.equal(calls.length,asked,'Another network asks again');assert.equal(connect.textContent,'Confirm switch');
 clock+=400;await ctx.connectSavedNetwork(connect);assert.deepEqual(calls.at(-1).body,{command:'connectWifi',value:2});assert.equal(connect.textContent,'Connect');assert.equal(connect.style.minWidth,'');
 assert.match(el('wifiMessage').textContent,/Restarting on “Garden”/);assert(el('connectWifi').disabled);assert(el('forgetWifi').disabled);
 refreshSucceeds=false;expireTimers();assert.equal(el('forgetWifi').disabled,false,'Release controls even when restart refresh fails');assert.equal(el('newSsid').disabled,false);assert.equal(el('connectWifi').disabled,false);
 const beforeForget=calls.length;await ctx.resetWifi(el('forgetWifi'));clock+=100;await ctx.resetWifi(el('forgetWifi'));assert.equal(calls.length,beforeForget,'A double click cannot forget the networks');
 clock+=400;await ctx.resetWifi(el('forgetWifi'));assert.equal(calls.at(-1).url,'/wifi/reset');assert.equal(el('forgetWifi').disabled,true);
 assert.match(el('wifiMessage').textContent,/forgotten.*“Matrix\.64-Setup”/,'Forgetting reports where to reconnect, and stays until the next action');
 expireTimers();assert.equal(el('forgetWifi').disabled,false,'Reset recovery must also release controls before refreshing');assert.equal(el('newPass').disabled,false);assert.equal(vm.runInContext('wifiBusy',ctx),false);
 console.log('PASS About with firmware and setup-network address, saved-network selector, retained selection, explicit save-only request, persistent feedback, storage/JSON/timeout failures, deliberate confirmations, no duplicate actions');
 await setupJoin();
})().catch(e=>{console.error(e);process.exitCode=1});
// The setup page sends one network at a time, stays disabled while the matrix restarts, and tells a failed scan from an empty one.
async function setupJoin(){
 const parts={},part=id=>parts[id]||={value:'',textContent:'',disabled:false,style:{},replaceChildren(...options){this.options=options}},posts=[],scans=[];
 let finish,scan={success:true,networks:[]},answer={success:true},pending=0;
 const page=vm.createContext({document:{getElementById:part},Option:class{constructor(text,value){this.text=text;this.value=value}},setTimeout:callback=>setImmediate(callback),async fetch(url,opts){
  if(url.startsWith('/wifi/scan')){scans.push(url);return {json:async()=>pending-->0?{success:true,scanning:true}:scan}}
  posts.push(JSON.parse(opts.body));await new Promise(resolve=>finish=resolve);return {ok:answer.success,json:async()=>answer};
 }});
 vm.runInContext(web.read('setup.html').split('<script>')[1].split('</script>')[0],page);
 await new Promise(resolve=>setImmediate(resolve));assert.match(part('msg').textContent,/No networks found/);
 // The page keeps asking while the matrix scans; Scan again starts a new scan, then waits on it.
 scans.length=0;pending=2;scan={success:true,networks:[{ssid:'Studio',secure:true}]};await page.scan(true);
 assert.deepEqual(scans,['/wifi/scan?again=1','/wifi/scan','/wifi/scan']);assert.deepEqual(part('ssid').options.map(o=>o.value),['','Studio']);assert.equal(part('msg').textContent,'');
 pending=1e9;await page.scan();assert.match(part('msg').textContent,/Scan failed/,'A scan that never finishes fails');pending=0;
 scan={success:false,networks:[]};await page.scan();assert.match(part('msg').textContent,/Scan failed/);
 part('manual').value='Studio';part('pass').value='test-only';
 answer={success:false,message:'Storage full'};let attempt=page.save();finish();await attempt;
 assert.equal(part('join').disabled,false,'A failed save can be retried');assert.equal(part('msg').textContent,'Storage full');
 answer={success:true};
 const first=page.save();assert.equal(part('join').disabled,true);await page.save();assert.equal(posts.length,2,'A second Join waits for the first');
 finish();await first;assert.equal(part('join').disabled,true,'Join stays off while the matrix restarts');assert.match(part('msg').textContent,/Saved/);
 assert.deepEqual(posts[1],{ssid:'Studio',password:'test-only',restart:true});
 console.log('PASS setup page: background scans awaited, one Join at a time, held during the restart, failed scans told apart');
}
