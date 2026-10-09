// One-time append; never reorder stable IDs or migrate user presets/bookmarks.
const fs=require('fs'),path=require('path');process.chdir(path.resolve(__dirname,'..'));
let ino=fs.readFileSync('64x64_matrix_01.ino','utf8');
const meta=JSON.parse(ino.match(/const pfMeta=(.*);/)[1]),authors=JSON.parse(ino.match(/const pfAuthors=(.*);/)[1]),ids=JSON.parse(ino.match(/const collectionPatternIds=(.*);/)[1]);
const catalog=JSON.parse(fs.readFileSync('assets/pattern-catalog.json'));
if(meta.length!==59||catalog.length!==55)throw Error('Expected 55 patterns; do not repeat');
meta[45][1]=['Bloom speed','Particle count','Petal folding','Counterpoint'];
const name='Whitney — Resonant Orbits v2';meta.push([name,['Canon speed','Particle count','Rate divergence','Echo length']]);
authors.push('Original C++ · John Whitney inspiration');ids.push(59);catalog.push({name,engine:16,variant:59});
ino=ino.replace(/const pfMeta=.*;/,'const pfMeta='+JSON.stringify(meta)+';').replace(/const pfAuthors=.*;/,'const pfAuthors='+JSON.stringify(authors)+';')
 .replace(/const collectionPatternIds=.*;/,'const collectionPatternIds='+JSON.stringify(ids)+';')
 .replace(/const uint8_t patternflowRenderSize\[PATTERNFLOW_PATTERN_COUNT\] = \{([^}]+)\}/,(_,s)=>'const uint8_t patternflowRenderSize[PATTERNFLOW_PATTERN_COUNT] = {'+s+',64}')
 .replace('>55 patterns<','>56 patterns<').replace('state.catalog?.length||55','state.catalog?.length||56').replace("'pong-55-24'","'orbits-v2-56-24'");
fs.writeFileSync('64x64_matrix_01.ino',ino);fs.writeFileSync('assets/pattern-catalog.json',JSON.stringify(catalog,null,2)+'\n');
let h=fs.readFileSync('src/generated/PatternCatalog.h','utf8').replace('COLLECTION_PATTERN_COUNT=45','COLLECTION_PATTERN_COUNT=46').replace(/COLLECTION_PATTERN_IDS\[COLLECTION_PATTERN_COUNT\]=\{[^}]+\}/,'COLLECTION_PATTERN_IDS[COLLECTION_PATTERN_COUNT]={'+ids+'}');fs.writeFileSync('src/generated/PatternCatalog.h',h);
console.log('Appended variant 59; existing IDs and presets untouched.');
