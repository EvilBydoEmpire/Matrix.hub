// Recorded previews: atlas coordinates, brightest-frame posters, reduced motion and where motion runs.
const assert=require('node:assert/strict'),vm=require('node:vm'),source=require('./web_source.cjs').read('previews.js');
function preview(size,reduced=false,pixels=null){
 const c=vm.createContext({state:{previewWidth:size,previewHeight:size,previewFrames:24,previewIntervalMs:67},Image:class{},requestAnimationFrame(){},
  window:{matchMedia:()=>({matches:reduced})},
  document:{hidden:false,querySelectorAll:()=>[],createElement:()=>({getContext:()=>({drawImage(){},getImageData:()=>({data:pixels})})})}});
 vm.runInContext(source+'\npreviewAtlasReady=true',c);return c;
}
const canvas=(index,draws)=>({width:300,height:150,dataset:{preview:String(index)},getContext:()=>({drawImage:(...v)=>draws.push(v.slice(1)),fillRect(){}})});
for(const size of [16,28]){
 const draws=[],target=canvas(9,draws);preview(size).drawPreview(target,2);
 assert.deepEqual([target.width,target.height],[size,size],`A canvas holds one ${size}px tile`);
 assert.deepEqual(draws[0],[2*size,9*size,size,size,0,0,size,size],`${size}px atlas coordinates, drawn pixel for pixel`);
}
// Two patterns of three 2×2 frames: row 0 is brightest in frame 2, row 1 in frame 0.
const width=6,height=4,pixels=new Uint8ClampedArray(width*height*4);
for(const [row,frame] of [[0,2],[1,0]])for(let y=row*2;y<row*2+2;y++)for(let x=frame*2;x<frame*2+2;x++)pixels.fill(200,(y*width+x)*4,(y*width+x)*4+3);
const posters=preview(2,false,pixels);
vm.runInContext(`state.previewFrames=3;previewAtlas.width=${width};previewAtlas.height=${height}`,posters);
assert.deepEqual(JSON.parse(JSON.stringify(vm.runInContext('findPosterFrames()',posters))),[2,0],'Posters use the brightest frame');
// Reduced motion keeps every preview on its poster.
const still=preview(28,true),stillDraws=[],stillCanvas=canvas(4,stillDraws);
vm.runInContext('posterFrames=[0,0,0,0,7]',still);
still.setAnimated(stillCanvas,true);
assert.equal(stillCanvas.dataset.animate,undefined);assert.deepEqual(stillDraws[0].slice(0,2),[7*28,4*28]);
// Only animated canvases move; posters skip them; stopping returns to the poster.
const live=preview(28),movingDraws=[],posterDraws=[],moving=canvas(1,movingDraws),poster=canvas(2,posterDraws);
live.setAnimated(moving,true);assert.equal(moving.dataset.animate,'1');
live.document.querySelectorAll=selector=>selector==='canvas[data-animate]'?[moving]:[];
live.animatePreviews(134);
assert.deepEqual(movingDraws.at(-1).slice(0,2),[2*28,1*28]);assert.equal(posterDraws.length,0);
live.drawPosters({querySelectorAll:()=>[moving,poster]});
assert.equal(posterDraws.length,1);assert.equal(movingDraws.length,1,'Posters leave animated canvases alone');
live.setAnimated(moving,false);assert.equal(moving.dataset.animate,undefined);assert.deepEqual(movingDraws.at(-1).slice(0,2),[0,28]);
// A failed atlas load retries by itself, backing off, and yields to a newer atlas.
{
 const timers=[],retry=preview(16);retry.setTimeout=(fn,ms)=>timers.push([fn,ms]);
 vm.runInContext("loadPreviewAtlas('a')",retry);assert.equal(vm.runInContext('previewAtlas.src',retry),'/preview-atlas.png?v=a');
 vm.runInContext('previewAtlas.src="";previewAtlas.onerror()',retry);assert.equal(timers.at(-1)[1],5000);
 timers.at(-1)[0]();assert.equal(vm.runInContext('previewAtlas.src',retry),'/preview-atlas.png?v=a','The same atlas is asked for again');
 vm.runInContext('previewAtlas.onerror()',retry);assert.equal(timers.at(-1)[1],10000,'Retries back off');
 vm.runInContext("loadPreviewAtlas('b')",retry);timers.at(-1)[0]();assert.equal(vm.runInContext('previewAtlas.src',retry),'/preview-atlas.png?v=b','A newer atlas is not replaced');
}
console.log('PASS 16px and 28px atlas coordinates drawn pixel for pixel, brightest-frame posters, reduced motion, motion only where it is asked for, and atlas retries');
