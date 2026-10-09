const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync('64x64_matrix_01.ino','utf8');
const names=source.split('\n').find(l=>l.startsWith('const paletteNames='));
assert(names);const ctx=vm.createContext({});
vm.runInContext(names+'\nthis.names=paletteNames',ctx);
assert.equal(ctx.names.length,21);
assert.deepEqual(Array.from(ctx.names.slice(11)),Array.from({length:10},(_,i)=>`Soukan ${i+1}`));
assert.equal(ctx.names[7],'Original','Legacy palette IDs must not shift');
const animate=source.split('\n').find(l=>l.startsWith('let previewLast='));
for(const size of [16,28]){
 const draws=[],canvas={width:28,height:28,dataset:{preview:'9'},getContext(){return{drawImage(...v){draws.push(v)},fillRect(){}}}};
 const c=vm.createContext({state:{previewWidth:size,previewHeight:size,previewFrames:24,previewIntervalMs:67},previewAtlasReady:true,previewAtlas:{},document:{querySelectorAll(){return[canvas]}},requestAnimationFrame(){}});
 vm.runInContext(animate+'\nanimatePatternPreviews(134)',c);
 assert.deepEqual(draws[0].slice(1),[2*size,9*size,size,size,0,0,28,28]);
}
console.log('PASS 21 stable palette IDs, Soukan labels, 16px and 28px gallery atlas coordinates');
