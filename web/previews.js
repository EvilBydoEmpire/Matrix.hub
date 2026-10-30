// Recorded previews: still posters in the library, motion only where attention rests.
const previewAtlas=new Image();let previewAtlasReady=false,previewAtlasKey=null,posterFrames=[],previewLast=0;
const reducedMotion=window.matchMedia('(prefers-reduced-motion: reduce)');
previewAtlas.onload=()=>{previewRetry=5000;posterFrames=findPosterFrames();previewAtlasReady=true;drawPosters(document)};
let previewRetry=5000;
previewAtlas.onerror=()=>{const key=previewAtlasKey;previewAtlasKey=null;setTimeout(()=>{if(previewAtlasKey===null)loadPreviewAtlas(key)},previewRetry);previewRetry=Math.min(previewRetry*2,60000)};
function loadPreviewAtlas(key){if(key===previewAtlasKey)return;previewAtlasKey=key;previewAtlasReady=false;previewAtlas.src='/preview-atlas.png?v='+key}
function previewSize(){return [state?.previewWidth||28,state?.previewHeight||28]}
// A pattern's poster is its brightest recorded frame; some begin dark.
function findPosterFrames(){
 const [pw,ph]=previewSize(),frames=state?.previewFrames||24,width=previewAtlas.width,height=previewAtlas.height;
 try{
  const canvas=document.createElement('canvas');canvas.width=width;canvas.height=height;
  const ctx=canvas.getContext('2d',{willReadFrequently:true});ctx.drawImage(previewAtlas,0,0);
  const data=ctx.getImageData(0,0,width,height).data,best=[];
  for(let row=0;row<Math.floor(height/ph);row++){
   let top=-1,pick=0;
   for(let frame=0;frame<frames;frame++){
    let sum=0;
    for(let y=row*ph;y<row*ph+ph;y++)for(let i=(y*width+frame*pw)*4,end=i+pw*4;i<end;i+=4)sum+=data[i]+data[i+1]+data[i+2];
    if(sum>top){top=sum;pick=frame}
   }
   best.push(pick);
  }
  return best;
 }catch(e){return []}
}
function posterFrame(index){return posterFrames[index]??0}
// Canvases hold one atlas tile pixel for pixel; CSS scales them up.
function drawPreview(canvas,frame){
 const [pw,ph]=previewSize();if(canvas.width!==pw||canvas.height!==ph){canvas.width=pw;canvas.height=ph}
 const ctx=canvas.getContext('2d');
 if(!previewAtlasReady){ctx.fillStyle='#111';ctx.fillRect(0,0,pw,ph);return}
 ctx.drawImage(previewAtlas,frame*pw,Number(canvas.dataset.preview)*ph,pw,ph,0,0,pw,ph);
}
function drawPosters(root){root.querySelectorAll('canvas[data-preview]').forEach(canvas=>{if(!canvas.dataset.animate)drawPreview(canvas,posterFrame(Number(canvas.dataset.preview)))})}
function setAnimated(canvas,on){
 if(on&&!reducedMotion.matches)canvas.dataset.animate='1';
 else{delete canvas.dataset.animate;drawPreview(canvas,posterFrame(Number(canvas.dataset.preview)))}
}
function animatePreviews(now){
 const interval=state?.previewIntervalMs||67;
 if(!document.hidden&&now-previewLast>=interval){
  previewLast=now-(now%interval);const frame=Math.floor(now/interval)%(state?.previewFrames||24);
  document.querySelectorAll('canvas[data-animate]').forEach(canvas=>drawPreview(canvas,frame));
 }
 requestAnimationFrame(animatePreviews);
}
