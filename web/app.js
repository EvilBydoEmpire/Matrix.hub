// Views, rendering and start-up.
let view='playing',lastView='playing';
function go(next){location.hash=next}
function showView(){
 const next=location.hash.slice(1),previous=view;view=['library','device'].includes(next)?next:'playing';if(view!=='device')lastView=view;
 if(view!==previous)window.scrollTo(0,0);
 document.body.dataset.view=view;$('deviceButton').textContent=view==='device'?'Done':'Device';
 for(const tab of document.querySelectorAll('.tabs button'))tab.setAttribute('aria-current',tab.dataset.view===view?'page':'false');
 if(view==='device')refreshDiagnostics();
}
function toggleDeviceView(){go(view==='device'?lastView:'device')}
function updateActivePattern(){
 const item=activeLibraryItem();if(!item)return;
 const test=testView(),canvas=$('activePreview');
 $('activePatternName').textContent=test||item.name;
 $('activePatternNote').hidden=!test;canvas.hidden=$('playingControls').hidden=Boolean(test);
 if(canvas.dataset.preview!==String(item.previewIndex)){canvas.dataset.preview=item.previewIndex;canvas.setAttribute('aria-label',item.name);setAnimated(canvas,true)}
}
function render(forceControls=false){
 $('deviceTitle').textContent=document.title=state.deviceName||'Matrix';
 updateActivePattern();renderPatternGallery();renderPlayMode();
 const key=selectedGalleryKey(),editing=pointerEditing||Object.keys(timers).length||document.activeElement?.closest?.('.control');
 if(forceControls||!editing||renderedPatternKey!==key){
  const focus=document.activeElement,within=$('playingControls').contains(focus)&&(focus.type==='radio'?`[name="${focus.name}"][value="${focus.value}"]`:focus.id&&'#'+focus.id);
  renderParams();renderedPatternKey=key;if(within)document.querySelector(within)?.focus();
 }
 if(!timers.brightness&&document.activeElement!==$('brightness'))$('brightness').value=state.brightness;
 $('brightnessValue').textContent=$('brightness').value;paintRange($('brightness'));
 renderSchedule();renderCalibration();renderAbout();
 $('app').inert=false;connectionStatus(true);
}
// Poll gently while visible.
function heartbeat(){if(!document.hidden)refreshDiagnostics();setTimeout(heartbeat,2000)}
function start(){
 showView();window.addEventListener('hashchange',showView);
 document.addEventListener('focusout',e=>{if(e.target.closest?.('.control,#scheduleFields'))setTimeout(()=>refreshState(),0)});
 document.addEventListener('pointerdown',e=>{if(e.target.closest?.('.control'))pointerEditing=true});
 for(const name of ['pointerup','pointercancel'])document.addEventListener(name,()=>{if(pointerEditing){pointerEditing=false;setTimeout(()=>refreshState(),0)}});
 document.addEventListener('visibilitychange',()=>{if(!document.hidden)refreshDiagnostics()});
 refreshState();heartbeat();requestAnimationFrame(animatePreviews);
}
window.addEventListener('load',start);
