// Library: every pattern in one grid; a tile plays its pattern.
function selectedGalleryKey(){return state&&state.currentPattern===16?'p'+Number(state.params.variant||0):'n'+Number(state?state.currentPattern:0)}
function activeLibraryItem(){return galleryItems.find(item=>item.key===selectedGalleryKey())}
function testView(){return state.calibration?.chart?'Test chart':null}
function makeTile(item){
 const tile=element('button','tile'),canvas=document.createElement('canvas');
 tile.type='button';tile.dataset.key=item.key;
 canvas.dataset.preview=item.previewIndex;
 tile.append(canvas,element('span','tileName',item.name));
 tile.onclick=()=>selectGalleryPattern(item);
 for(const [event,on] of [['pointerenter',true],['pointerleave',false],['focus',true],['blur',false]])tile.addEventListener(event,()=>setAnimated(canvas,on));
 return tile;
}
function renderPatternGallery(){
 const root=$('patternGallery');
 if(!root.childElementCount){root.replaceChildren(...galleryItems.map(makeTile));drawPosters(root)}
 const selected=selectedGalleryKey(),tiles=[...root.children];
 for(const tile of tiles)tile.setAttribute('aria-current',String(tile.dataset.key===selected));
 if(!root.contains(document.activeElement))focusTile(root,tiles.find(tile=>tile.dataset.key===selected)||tiles[0],false);
}
// The grid is one tab stop; arrows, Home and End move between tiles.
function focusTile(root,target,move=true){
 for(const tile of root.children)tile.tabIndex=tile===target?0:-1;
 if(move)target.focus();
}
function moveTileFocus(e){
 const root=e.currentTarget,tiles=[...root.children],i=tiles.indexOf(document.activeElement);if(i<0||e.altKey||e.ctrlKey||e.metaKey)return;
 let next;
 if(e.key==='ArrowLeft')next=tiles[i-1];else if(e.key==='ArrowRight')next=tiles[i+1];else if(e.key==='Home')next=tiles[0];else if(e.key==='End')next=tiles.at(-1);
 else if(e.key==='ArrowUp'||e.key==='ArrowDown'){
  const from=tiles[i].getBoundingClientRect(),down=e.key==='ArrowDown',tops=tiles.map(tile=>tile.getBoundingClientRect().top).filter(top=>down?top>from.top+1:top<from.top-1);
  const row=down?Math.min(...tops):Math.max(...tops),distance=tile=>Math.abs(tile.getBoundingClientRect().left-from.left);
  next=tiles.filter(tile=>tile.getBoundingClientRect().top===row).sort((a,b)=>distance(a)-distance(b))[0];
 }
 if(!next)return;e.preventDefault();focusTile(root,next);
}
async function selectGalleryPattern(item){
 if(item.key===selectedGalleryKey()&&!testView())return;
 if(item.variant===null)await command('setPattern',{value:item.engine});else await command('setPatternVariant',{value:item.variant});
}
