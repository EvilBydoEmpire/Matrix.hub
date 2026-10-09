const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync('64x64_matrix_01.ino','utf8');
// Exercise the exact firmware attempt-order expressions, translated only in
// their local variable declarations. Every stored slot is visited once.
const loop=source.split('// Selected network first;')[1].split('\n').slice(1).join('\n').split('    String ssid;')[0].replaceAll('int attempt','let attempt').replaceAll('int i=','let i=');
const order=new Function('count','preferred','const out=[];'+loop+'out.push(i);}return out;');
for(let n=0;n<=8;n++)for(let p=-1;p<=10;p++){const got=order(n,p);assert.equal(got.length,n);assert.equal(new Set(got).size,n);assert(got.every(i=>i>=0&&i<n));if(p>=0&&p<n)assert.equal(got[0],p)}
const script=source.split('// WIFI_CONTROLS_START')[1].split('\n').slice(1).join('\n').split('// WIFI_CONTROLS_END')[0];
const els={};function el(id){return els[id]||=(id==='savedWifi'?{value:'',options:[],replaceChildren(){this.options=[];this.value=''},appendChild(o){this.options.push(o);if(this.options.length===1)this.value=o.value}}:{value:'',textContent:'',style:{}})}
const calls=[];let response={success:true};let fail=null;
const state={networkMode:'station',ssid:'Home',wifiConnected:true,hostname:'64x64-rgb-matrix',savedNetworks:[{index:0,ssid:'Home',connected:true},{index:1,ssid:'Studio <test>',preferred:true}]};
const ctx=vm.createContext({$:el,state,document:{createElement(){return{value:'',textContent:''}}},AbortController,
 setTimeout(){return 1},clearTimeout(){},async refreshState(){ctx.renderSavedNetworks()},async fetch(url,opts){calls.push({url,body:JSON.parse(opts.body)});if(fail)throw fail;return{ok:response.success!==false,async json(){if(response==='invalid')throw Error('invalid');return response}}}});
vm.runInContext(script,ctx);
(async()=>{
 ctx.renderSavedNetworks();assert.equal(el('savedWifi').value,'0');assert.equal(el('savedWifi').options[1].textContent,'Studio <test> · preferred');
 el('savedWifi').value='1';ctx.renderSavedNetworks();assert.equal(el('savedWifi').value,'1','Refresh must preserve selection');
 await ctx.addNetwork();assert.equal(calls.length,0);assert.match(el('wifiMessage').textContent,/Enter/);
 el('newSsid').value='Studio <test>';el('newPass').value='test-only';
 await ctx.addNetwork();assert.equal(calls.at(-1).body.restart,false);assert.equal(el('newPass').value,'');assert.match(el('wifiMessage').textContent,/Saved/);assert.equal(el('savedWifi').value,'1');
 el('newSsid').value='Unsaved';el('newPass').value='retain-on-error';response={success:false,message:'Storage full'};
 await ctx.addNetwork();assert.match(el('wifiMessage').textContent,/Storage full/);assert.equal(el('newPass').value,'retain-on-error');assert.equal(el('saveWifi').disabled,false);
 response='invalid';await ctx.addNetwork();assert.match(el('wifiMessage').textContent,/No confirmation/);assert.equal(el('newSsid').value,'Unsaved');
 fail=Object.assign(new Error(),{name:'AbortError'});await ctx.addNetwork();assert.match(el('wifiMessage').textContent,/12 seconds/);fail=null;
 response={success:true};el('savedWifi').value='1';await ctx.connectSavedNetwork();assert.deepEqual(calls.at(-1).body,{command:'connectWifi',value:1});assert.match(el('wifiMessage').textContent,/Switch accepted/);assert(el('connectWifi').disabled);
 console.log('PASS saved-network selector, retained selection, explicit save-only request, persistent feedback, storage/JSON/timeout failures, confirmed switch, no duplicate actions');
})().catch(e=>{console.error(e);process.exitCode=1});
