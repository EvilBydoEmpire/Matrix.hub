// Guarded one-time catalog append. Stable identities and existing stars remain.
const fs=require('fs'),path=require('path');process.chdir(path.resolve(__dirname,'..'));
const variants=[
 ['Whitney — Motion Graphics 1968',['Motion speed','Line count','Phase spacing','Pause duration'],'John Whitney · Kenny Kim recreation'],
 ['Whitney — Differential Columns',['Motion speed','Orbit radius','Phase divergence','Dot size'],'John Whitney study · user-supplied sketch'],
 ['Whitney — Chromatic Bloom',['Wave speed','Tile count','Radial travel','Phase scatter'],'John Whitney study · user-supplied sketch']
];
let ino=fs.readFileSync('64x64_matrix_01.ino','utf8');
const meta=JSON.parse(ino.match(/const pfMeta=(.*);/)[1]);
const authors=JSON.parse(ino.match(/const pfAuthors=(.*);/)[1]);
const ids=JSON.parse(ino.match(/const collectionPatternIds=(.*);/)[1]);
const catalog=JSON.parse(fs.readFileSync('assets/pattern-catalog.json','utf8'));
if(meta.length!==43||catalog.length!==46)throw Error('Already registered or catalog changed; do not replay');
for(let i=0;i<variants.length;i++){
 const [name,labels,author]=variants[i];meta.push([name,labels]);authors.push(author);ids.push(43+i);
 catalog.push({name,engine:16,variant:43+i});
}
ino=ino.replace(/const pfMeta=.*;/,'const pfMeta='+JSON.stringify(meta)+';')
 .replace(/const pfAuthors=.*;/,'const pfAuthors='+JSON.stringify(authors)+';')
 .replace(/const collectionPatternIds=.*;/,'const collectionPatternIds='+JSON.stringify(ids)+';')
 .replace(/const uint8_t patternflowRenderSize\[PATTERNFLOW_PATTERN_COUNT\] = \{([^}]+)\}/,(_,sizes)=>'const uint8_t patternflowRenderSize[PATTERNFLOW_PATTERN_COUNT] = {'+sizes+',32,32,32}')
 .replace('>46 patterns<','>49 patterns<').replace('state.catalog?.length||46','state.catalog?.length||49').replace("'curated-46-24'","'whitney-49-24'");
fs.writeFileSync('64x64_matrix_01.ino',ino);
fs.writeFileSync('assets/pattern-catalog.json',JSON.stringify(catalog,null,2)+'\n');
console.log('Appended 3 Whitney studies; 49 active patterns. No bookmark migration needed.');
