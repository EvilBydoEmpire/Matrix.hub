// The app's behaviour without a browser or hardware, and its contracts with the firmware.
const assert=require('node:assert/strict'),web=require('./web_source.cjs');
const {tick,settle}=web;
const ok={ok:true,status:200,json:async()=>({success:true,message:'ok'})};

function controlRows(){
 const app=web.loadApp(),{ctx,run,advance}=app,calls=[];
 run('state={currentPattern:16,params:{variant:50,c0:5}}');
 ctx.command=(command,extra)=>calls.push(app.plain({command,...extra}));
 const row=ctx.makeSlider(web.node(),['c0','Control 1',0,100,1],5,16),range=row.querySelector('input'),output=row.querySelector('output');
 assert.equal(Number(range.value),5);
 assert.equal(output.textContent,'5');
 assert.equal(calls.length,0,'Rendering must not send commands');

 range.value='7';range.oninput();
 assert.equal(calls.length,0,'Edits wait for a short pause');
 range.value='8';range.oninput();
 assert.equal(output.textContent,'8','The readout follows the slider at once');
 advance(130);
 assert.deepEqual(calls,[{command:'setParam',paramName:'c0',value:8,expectedPattern:16,expectedVariant:50}],
  'A burst of input sends only its last value with the captured identity');
 assert.equal(run('state.params.c0'),8,'The current pattern shows the edit immediately');

 const beforeDrag=calls.length;
 for(let v=0;v<=10;v++){range.value=String(v);range.oninput();advance(40)}
 assert(calls.length>beforeDrag,'A continuous drag still updates the matrix while moving');
 advance(130);
 assert.equal(calls.at(-1).value,10,'The final drag position is always sent');
 const sent=calls.length;
 advance(1000);
 assert.equal(calls.length,sent,'No commands follow once the edit settles');

 range.value='999';range.oninput();advance(130);
 assert.equal(calls.at(-1).value,100,'Values are clamped to the range');
 range.value='2.4';range.oninput();advance(130);
 assert.equal(calls.at(-1).value,2,'Values snap to the parameter step');

 run('state.params.variant=51');
 range.value='4';range.oninput();advance(130);
 assert.equal(calls.at(-1).expectedVariant,50,'A stale row keeps its original identity');
 assert.equal(run('state.params.c0'),2,'A stale row leaves the newly selected pattern untouched');
 run('state.params.variant=50');

 const percent=ctx.makeSlider(web.node(),['saturation','Saturation',0,200,1],90,16,v=>v+' %');
 assert.equal(percent.querySelector('output').textContent,'90 %','Readouts show units');
 percent.querySelector('input').value='120';percent.querySelector('input').oninput();
 assert.equal(percent.querySelector('output').textContent,'120 %');
 assert.equal(run('state.params.saturation'),120);
 const fill=Object.assign(web.node(),{min:'0',max:'15',value:'3'});ctx.paintRange(fill);
 assert.equal(fill.style['--fill'],'20%','The filled track matches the value');

 const labels=rows=>app.plain(rows).map(row=>row[0]+':'+row[1]);
 assert.deepEqual(labels(ctx.controlsFor(16,50)),['c0:Flow speed','c1:Droplets','c2:Cohesion','c3:Metallic reflection']);
 assert.deepEqual(labels(ctx.controlsFor(16,5)),['c1:Speed','c2:Ribbons','c3:Amplitude'],'A control without a label stays hidden');
 for(const engine of app.plain(run('Object.keys(defs).map(Number)')))
  assert(ctx.controlsFor(engine,0).length>=2,`Engine ${engine} has its own controls`);
 assert.deepEqual([30,300,90,3600,5400,3601,7170,86399].map(ctx.formatDuration),['30 s','5 min','1 min 30 s','1 h','1 h 30 min','1 h','2 h','24 h']);

 // Brightness is one device setting: a drag shows at once and sends its last value.
 advance(1000);
 const before=calls.length;
 ctx.setBrightness('5');ctx.setBrightness('6');
 assert.equal(run('state.brightness'),6);
 assert.equal(app.el('brightnessValue').textContent,'6');
 advance(130);
 assert.deepEqual(calls.slice(before),[{command:'setBrightness',value:6}]);

 // The white balance is live like any slider and is not redrawn under a drag.
 for(const [id,value] of [['calR',90],['calG',80],['calB',70]])Object.assign(app.el(id),{min:'0',max:'100',value:String(value),previousElementSibling:web.node()});
 const beforeBalance=calls.length;
 ctx.editBalance(app.el('calR'));app.el('calR').value='85';ctx.editBalance(app.el('calR'));
 assert.equal(app.el('calR').previousElementSibling.textContent,'85 %');
 run('state.calibration={chart:false,r:100,g:100,b:100};renderCalibration()');
 assert.equal(app.el('calR').value,'85','A pending edit is not overwritten');
 advance(130);
 assert.deepEqual(calls.slice(beforeBalance),[{command:'setColorCalibration',value:{r:85,g:80,b:70}}]);
 // Display tuning shows whenever the matrix reports a balance, and Reset asks first, then returns to the panel's default.
 run('state.calibration=null;renderCalibration()');assert.equal(app.el('calibrationPanel').hidden,true);
 run('state.calibration={chart:false,r:85,g:80,b:70,default:{r:100,g:69,b:94}};renderCalibration()');assert.equal(app.el('calibrationPanel').hidden,false);
 const resetButton=web.node();resetButton.textContent='Reset white balance';const beforeReset=calls.length;
 ctx.resetColorCalibration(resetButton);assert.equal(calls.length,beforeReset,'The first press only arms the button');
 resetButton._armedAt=0;ctx.resetColorCalibration(resetButton);
 assert.deepEqual(calls.slice(beforeReset),[{command:'setColorCalibration',value:{r:100,g:69,b:94}}]);
 assert.equal(app.el('calG').previousElementSibling.textContent,'69 %');
 console.log('PASS control rows: coalesced live edits, captured identity, clamping, steps, readouts, labels, durations, brightness and white balance with its reset');
}

async function requests(){
 const {ctx,run,el,advance}=web.loadApp(),calls=[],messages=[];
 run('state={currentPattern:16,params:{variant:50}};refreshState=async()=>true');
 ctx.toast=(msg,ok=true)=>messages.push({msg,ok});
 // One write at a time, each carrying the pattern identity captured when it was issued.
 let release;ctx.fetch=async(url,opt)=>{calls.push({url,body:JSON.parse(opt.body)});await new Promise(r=>release=r);return ok};
 const first=ctx.post({command:'setParam',paramName:'c0',value:6});
 const second=ctx.post({command:'resetPattern'});
 run('state.params.variant=51');await tick();assert.equal(calls.length,1,'Only one request may be in flight');
 release();await first;await tick();assert.equal(calls.length,2);assert.equal(calls[1].body.expectedVariant,50,'Capture identity before queued execution');release();await second;
 run('state.params.variant=50');calls.length=0;
 ctx.fetch=async(_,opt)=>{calls.push(JSON.parse(opt.body));return {ok:false,status:400,json:async()=>({success:false,message:'Edit rejected'})}};
 await assert.rejects(ctx.post({command:'setParam',paramName:'c0',value:6}),/Edit rejected/);assert.equal(calls.length,1);
 ctx.fetch=async()=>({ok:true,status:200,json:async()=>({success:false,message:'Storage full'})});
 assert.equal(await ctx.command('resetPattern'),false);assert.equal(messages.at(-1).ok,false);
 ctx.fetch=async()=>({ok:true,status:200,json:async()=>{throw Error('bad')}});await assert.rejects(ctx.api('/control'),/sent no confirmation/);
 ctx.fetch=async()=>{throw Object.assign(Error(),{name:'AbortError'})};await assert.rejects(ctx.api('/control'),/did not answer/);

 // Schedule edits save as one command, captured before sending; a failed save keeps its draft until the next edit.
 el('scheduleFields').querySelectorAll=()=>['scheduleStart','scheduleEnd','scheduleFadeIn','scheduleFadeOut'].map(el);
 el('scheduleStart').value='09:30';el('scheduleEnd').value='23:15';el('scheduleFadeIn').value='30';el('scheduleFadeOut').value='45';el('scheduleEnabled').checked=true;
 calls.length=0;ctx.fetch=async(_,opt)=>{calls.push(JSON.parse(opt.body));el('scheduleFadeIn').value='1';return {ok:false,status:400,json:async()=>({success:false,message:'Storage full'})}};
 ctx.editSchedule();assert.equal(calls.length,0,'Edits wait for a short pause');ctx.flushEdits();await settle();
 assert.deepEqual(calls,[{command:'setSchedule',enabled:true,startMinutes:570,endMinutes:1395,fadeInSeconds:30,fadeOutSeconds:45}]);
 assert.equal(run('scheduleDirty'),true);assert.deepEqual(messages.at(-1),{msg:'Storage full',ok:false});
 run('state.scheduleEnabled=true;state.scheduleFadeInSeconds=99;renderSchedule()');assert.equal(el('scheduleFadeIn').value,'1','Refreshes leave an unsaved draft alone');
 ctx.fetch=async()=>ok;el('scheduleFadeIn').value='30';ctx.editSchedule();ctx.flushEdits();await settle();
 assert.equal(run('scheduleDirty'),false,'The next edit retries and clears the draft');
 calls.length=0;el('scheduleFadeIn').valid=false;ctx.fetch=async(_,opt)=>{calls.push(JSON.parse(opt.body));return ok};
 ctx.editSchedule();ctx.flushEdits();await settle();assert.equal(calls.length,0,'An invalid field is reported, not sent');assert.equal(el('scheduleFadeIn').reported,true);
 el('scheduleFadeIn').reported=false;run('state.scheduleFadeInSeconds=20');el('scheduleEnabled').checked=false;ctx.editSchedule();
 assert.equal(el('scheduleFields').hidden,true,'Turning the schedule off folds its times away');assert.equal(el('scheduleFadeIn').reported,false,'Hidden fields are not validated');
 ctx.flushEdits();await settle();el('scheduleFadeIn').valid=true;
 assert.deepEqual(calls,[{command:'setSchedule',enabled:false,startMinutes:570,endMinutes:1395,fadeInSeconds:20,fadeOutSeconds:45}],'A hidden invalid field sends its saved value');
 run('Object.assign(state,{scheduleEnabled:true,scheduleStartMinutes:600,scheduleEndMinutes:1395,scheduleFadeInSeconds:30,scheduleFadeOutSeconds:45})');
 ctx.document.activeElement=el('scheduleStart');el('scheduleFields').contains=node=>node===el('scheduleStart');
 ctx.renderSchedule();assert.equal(el('scheduleStart').value,'09:30','A refresh leaves the field being edited alone');
 ctx.document.activeElement=null;ctx.renderSchedule();assert.equal(el('scheduleStart').value,'10:00');
 // A network restart waits behind a schedule save and pending slider edits.
 const ordered=[];let releaseSave;
 ctx.fetch=async(_,opt)=>{ordered.push(JSON.parse(opt.body).command);if(ordered.length===1)await new Promise(r=>releaseSave=r);return ok};
 ctx.editSchedule();ctx.flushEdits();await tick();
 ctx.debounce('pending',()=>ctx.command('setParam',{paramName:'c0',value:7}));
 const restart=ctx.wifiRequest('/control',{command:'connectWifi',value:1});
 assert.equal(ordered.length,1,'Network restart must wait behind the schedule save');
 releaseSave();await restart;
 assert.deepEqual(ordered,['setSchedule','setParam','connectWifi']);

 // Reset asks on its button, keeps its width while asking, and confirms only a deliberate second press for the same pattern.
 const resetCalls=[],resetButton=Object.assign(web.node(),{textContent:'Reset to defaults',offsetWidth:180});
 ctx.fetch=async(_,opt)=>{resetCalls.push(JSON.parse(opt.body));return ok};
 await ctx.resetPattern(resetButton);assert.equal(resetCalls.length,0,'The first press only asks');
 assert.equal(resetButton.textContent,'Confirm reset');assert.equal(resetButton.style.minWidth,'180px','The armed button keeps its width');
 advance(4000);assert.equal(resetButton.textContent,'Reset to defaults','An unanswered question lapses');assert.equal(resetButton.style.minWidth,'');
 await ctx.resetPattern(resetButton);advance(100);await ctx.resetPattern(resetButton);
 assert.equal(resetCalls.length,0,'A double click or key repeat only asks');assert.equal(resetButton.textContent,'Confirm reset');
 run('state.params.variant=51');advance(400);await ctx.resetPattern(resetButton);
 assert.equal(resetCalls.length,0,'Another pattern asks again');assert.equal(resetButton.textContent,'Confirm reset');
 advance(400);await ctx.resetPattern(resetButton);assert.equal(resetButton.textContent,'Reset to defaults');
 assert.deepEqual([resetCalls[0].command,resetCalls[0].expectedPattern,resetCalls[0].expectedVariant],['resetPattern',16,51]);
 run('state.params.variant=50');

 // Every pattern fills one grid; a tile plays its pattern, and the playing one is marked.
 const libraryCalls=[];ctx.fetch=async(_,opt)=>{libraryCalls.push(JSON.parse(opt.body));return ok};
 run('state={currentPattern:16,params:{variant:50}}');ctx.renderPatternGallery();
 const tiles=el('patternGallery').children,tile=key=>tiles.find(t=>t.dataset.key===key);
 assert.equal(tiles.length,run('galleryItems.length'));
 assert.equal(ctx.activeLibraryItem(),run('galleryItems').find(item=>item.variant===50));
 assert.deepEqual(tiles.filter(t=>t['attr aria-current']==='true').map(t=>t.dataset.key),['p50']);
 assert.deepEqual(tiles.filter(t=>t.tabIndex===0).map(t=>t.dataset.key),['p50'],'The grid is one tab stop, on the playing pattern');
 await tile('p50').onclick();assert.equal(libraryCalls.length,0,'The playing pattern keeps playing');
 await tile('p65').onclick();await tile('n3').onclick();
 assert.deepEqual(libraryCalls,[{command:'setPatternVariant',value:65},{command:'setPattern',value:3}],'Every pattern plays at once');
 run('state={currentPattern:0,params:{}}');
 assert.equal(ctx.selectedGalleryKey(),'n0','Native engines key on their engine ID');
 assert.equal(ctx.testView(),null);
 run('state.calibration={chart:true}');assert.equal(ctx.testView(),'Test chart');

 // Palette swatches: colour palettes around the colour wheel, then the single tones, Original last; the active one shows this pattern's own stops.
 run('state={currentPattern:16,params:{variant:50,palette:9,color0:"#142139",color1:"#3d5a7f",color2:"#ede0c4",color3:"#c2673b",original:["#111111","#222222","#333333","#444444"]}}');
 ctx.makeSlider=()=>{};ctx.renderColorControls();
 const strips=el('colorControls').children[0].children,strip=id=>strips.find(option=>option.innerHTML.includes(`value="${id}"`));
 assert.deepEqual(strips.map(option=>[Number(option.innerHTML.match(/value="(\d+)"/)[1]),option.innerHTML.match(/<span>([^<]+)<\/span>/)[1]]),
  [[4,'Rosewood'],[21,'Peony'],[1,'Bonfire'],[0,'Ember'],[3,'Terracotta'],[8,'Patina'],[2,'Saffron'],[22,'Citrus'],[6,'Moss'],[23,'Forest'],[24,'Sage'],[25,'Aurora'],
   [26,'Lagoon'],[27,'Glacier'],[28,'Cobalt'],[9,'Ink'],[29,'Heather'],[30,'Orchid'],[31,'Neon'],[5,'Dusk'],[10,'Driftwood'],[32,'Silver'],
   [33,'Red'],[34,'Orange'],[35,'Green'],[36,'Cyan'],[37,'Blue'],[38,'Purple'],[39,'Magenta'],[7,'Original']]);
 assert.deepEqual(strips.filter(option=>option.querySelector('input').checked),[strip(9)]);
 assert.equal(strip(9).querySelector('i').style.background,'linear-gradient(90deg,#142139 0% 25%,#3d5a7f 25% 50%,#ede0c4 50% 75%,#c2673b 75% 100%)');
 assert.equal(strip(7).querySelector('i').style.background,'linear-gradient(90deg,#111111 0% 25%,#222222 25% 50%,#333333 50% 75%,#444444 75% 100%)','Original shows the authored colours');
 assert.equal(strip(0).querySelector('i').style.background,'linear-gradient(90deg,#4C110A 0% 25%,#A63C0C 25% 50%,#F1AF57 50% 75%,#D76300 75% 100%)');
 const paletteCalls=[];ctx.fetch=async(_,opt)=>{paletteCalls.push(JSON.parse(opt.body));return ok};
 await strip(0).querySelector('input').onchange();
 assert.deepEqual(paletteCalls,[{command:'setParam',paramName:'palette',value:0,expectedPattern:16,expectedVariant:50}]);
 console.log('PASS requests: one write at a time, captured identity, API failures, schedule drafts and focus, restart ordering, reset guard, library grid and palette strips');
}

// A poll that overlaps a rejected edit is replaced by one fresh read once writes settle.
async function staleRefresh(readFirst){
 const {ctx,run}=web.loadApp();let resolveRead,resolveWrite,reads=0;
 ctx.fetch=async(_,opt)=>{
  if(opt?.method==='POST'){await new Promise(resolve=>resolveWrite=resolve);return {ok:false,status:400,json:async()=>({success:false,message:'Rejected'})}}
  reads++;if(reads===1)await new Promise(resolve=>resolveRead=resolve);
  return {ok:true,status:200,json:async()=>({success:true,stateRevision:7,currentPattern:16,params:{variant:50,c0:7}})};
 };
 run('state={stateRevision:7,currentPattern:16,params:{variant:50,c0:9}};lastStateRevision=7;toast=()=>{};render=()=>{};renderSavedNetworks=()=>{}');
 const read=ctx.refreshState(),edit=ctx.command('setParam',{paramName:'c0',value:9});
 await tick();
 if(readFirst){resolveRead();await read;assert.equal(reads,1,'Wait for pending writes before fetching replacement state');resolveWrite()}
 else{resolveWrite();await tick();resolveRead()}
 await Promise.all([read,edit]);await tick();
 assert.equal(run('state.params.c0'),7,'A rejected optimistic edit must reconcile even without a new state revision');
 assert.equal(reads,2,'Replace a discarded GET with one fresh read after writes settle');
}

// Diagnostics that recover after a failure refresh state even when a reboot reused the revision.
async function reconnect(failedEndpoint){
 const {ctx,run}=web.loadApp(),calls=[];
 ctx.fetch=async url=>{
  calls.push(url);if(calls.length===1)throw Error('Network handoff');
  const data=url==='/control'?{success:true,stateRevision:7,currentPattern:16,params:{variant:50,c0:7}}
   :{success:true,stateRevision:7,actualFPS:60,panelRefreshHz:60,backend:'ws2812',renderBudgetPercent:50,freeHeap:1024};
  return {ok:true,status:200,json:async()=>data};
 };
 run('state={stateRevision:7,currentPattern:16,params:{variant:50,c0:9}};lastStateRevision=7;toast=()=>{};render=()=>{};renderSavedNetworks=()=>{}');
 if(failedEndpoint==='/control')await ctx.refreshState();else await ctx.refreshDiagnostics();
 await ctx.refreshDiagnostics();
 assert.deepEqual(calls,[failedEndpoint,'/diagnostics','/control']);
 assert.equal(run('state.params.c0'),7);
}

// Every command the page sends exists in the firmware, every inline handler in the scripts, and names, labels and order match.
function firmwareContracts(){
 const {run,plain}=web.loadApp(),script=web.script(),page=web.read('index.html'),source=web.readProject('src/main.cpp');
 const commands=new Set([...(script+page).matchAll(/(?:command\(|command:)\s*['"]([A-Za-z]+)['"]/g)].map(m=>m[1]));
 const backend=source.slice(source.indexOf('String processControlCommandJson(const String& body, int& status) {'),source.indexOf('void handleControlPost() {'));
 for(const name of commands)assert(backend.includes('"'+name+'"'),`Missing backend command: ${name}`);
 const handlers=new Set([...page.matchAll(/\son\w+="(?:[^"]*?;)?\s*(?:event\.preventDefault\(\);)?(?:await )?([A-Za-z]+)\(/g)].map(m=>m[1]));
 for(const name of handlers)assert(name==='command'||new RegExp(`function ${name}\\(`).test(script),`Missing handler: ${name}`);
 const enums=web.readProject('src/patterns/NativeEngines.h').split('enum Animation {')[1].split('};')[0].replace(/\/\/.*$/gm,'').split(',').map(s=>s.trim());
 const setter=source.split('bool setPatternParam(const char* name, JsonVariantConst value) {')[1].split('void sendJson(')[0];
 const getter=source.split('void addCurrentParamsToJson(JsonObject obj) {')[1].split('bool setPatternParam(')[0];
 const definitions=plain(run('defs'));
 for(const engine of Object.keys(definitions).map(Number)){
  const write=setter.split('case '+enums[engine]+':')[1].split('break;')[0];
  const read=getter.split('case '+enums[engine]+':')[1].split('break;')[0];
  for(const [key] of definitions[engine].params){assert(write.includes('"'+key+'"'),`Missing setter ${engine}/${key}`);assert(read.includes('"'+key+'"'),`Missing state field ${engine}/${key}`)}
 }
 for(const key of ['saturation','palette','original'])assert(getter.includes('"'+key+'"'),`Missing shared state field ${key}`);
 // A null label is a control kept at its default.
 const names=source.split('animationNames[NUM_ANIMATIONS] = {')[1].split('};')[0].split(',').map(s=>s.trim()).map(s=>s==='nullptr'?null:s.slice(1,-1));
 for(const [engine,definition] of Object.entries(definitions))assert.equal(definition.name,names[engine],`Engine ${engine} matches the firmware name`);
 const metadata=web.readProject('src/patterns/PatternflowPack.cpp').split('PATTERNFLOW_METADATA[PATTERNFLOW_PATTERN_COUNT] = {')[1].split('\n};')[0].trim().split('\n')
  .map(row=>{const match=row.trim().match(/^\{"([^"]*)", \{([^}]*)\}/);return match&&[match[1],match[2].split(',').map(s=>s.trim()).map(s=>s==='nullptr'?null:s.slice(1,-1))]});
 for(const [variant,entry] of Object.entries(plain(run('collection'))))assert.deepEqual(entry,metadata[variant],`Collection ${variant} matches the firmware`);
 const firmwareIds=name=>web.readProject('src/patterns/PatternCatalog.h').split(name+'[')[1].split('{')[1].split('}')[0].split(',').map(Number);
 const gallery=[...firmwareIds('CORE_PATTERN_IDS'),...firmwareIds('COLLECTION_PATTERN_IDS')];
 assert.deepEqual(plain(run('galleryItems.map(x=>x.variant??x.engine)')),gallery,'The library follows the firmware gallery order');
 console.log(`PASS firmware contracts: ${commands.size} commands, ${handlers.size} page handlers, names, labels and ${gallery.length} gallery entries`);
}

(async()=>{
 controlRows();
 await requests();
 for(const readFirst of [false,true])await staleRefresh(readFirst);
 for(const endpoint of ['/control','/diagnostics'])await reconnect(endpoint);
 console.log('PASS refreshes: stale polls reconcile and recover when a reboot reuses the revision');
 firmwareContracts();
})().catch(error=>{console.error(error);process.exitCode=1});
