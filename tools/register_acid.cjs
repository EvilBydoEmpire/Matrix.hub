// One-time mechanical catalog append. Never renumber existing IDs.
const fs=require('fs'),path=require('path');process.chdir(path.resolve(__dirname,'..'));
let ino=fs.readFileSync('64x64_matrix_01.ino','utf8');
const meta=JSON.parse(ino.match(/const pfMeta=(.*);/)[1]),authors=JSON.parse(ino.match(/const pfAuthors=(.*);/)[1]),ids=JSON.parse(ino.match(/const collectionPatternIds=(.*);/)[1]);
const catalog=JSON.parse(fs.readFileSync('assets/pattern-catalog.json'));
if(meta.length!==60||catalog.length!==56)throw Error('Expected 60 variants / 56 gallery entries; do not repeat');
for(const [i,label,warp] of [[60,'Acid Cathedral','Arch distortion'],[61,'Phase Slippage','Phase warp'],[62,'Liquid Circuit','Liquid warp']]){
 const name='Warped Checkerboard — '+label;
 meta.push([name,['Motion speed','Grid density',warp,'Aliasing']]);authors.push('Original C++ · Acid studies');ids.push(i);catalog.push({name,engine:16,variant:i});
}
ino=ino.replace(/const pfMeta=.*;/,'const pfMeta='+JSON.stringify(meta)+';').replace(/const pfAuthors=.*;/,'const pfAuthors='+JSON.stringify(authors)+';')
 .replace(/const collectionPatternIds=.*;/,'const collectionPatternIds='+JSON.stringify(ids)+';')
 .replace(/const uint8_t patternflowRenderSize\[PATTERNFLOW_PATTERN_COUNT\] = \{([^}]+)\}/,(_,s)=>'const uint8_t patternflowRenderSize[PATTERNFLOW_PATTERN_COUNT] = {'+s+',64,64,64}')
 .replace('>56 patterns<','>59 patterns<').replace('state.catalog?.length||56','state.catalog?.length||59').replace("'orbits-v2-56-24'","'acid-archive-59-24'");
fs.writeFileSync('64x64_matrix_01.ino',ino);fs.writeFileSync('assets/pattern-catalog.json',JSON.stringify(catalog,null,2)+'\n');
let h=fs.readFileSync('src/generated/PatternCatalog.h','utf8').replace('COLLECTION_PATTERN_COUNT=46','COLLECTION_PATTERN_COUNT=49').replace(/COLLECTION_PATTERN_IDS\[COLLECTION_PATTERN_COUNT\]=\{[^}]+\}/,'COLLECTION_PATTERN_IDS[COLLECTION_PATTERN_COUNT]={'+ids+'}');fs.writeFileSync('src/generated/PatternCatalog.h',h);
console.log('Appended variants 60–62; existing IDs/presets/bookmarks untouched.');
