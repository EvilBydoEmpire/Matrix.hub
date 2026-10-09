// Guarded one-shot revision 3: exact live removal manifest, stable IDs retained.
const fs=require('fs'),path=require('path');process.chdir(path.resolve(__dirname,'..'));
const snapshot=JSON.parse(fs.readFileSync('patterns/archive/pre-advanced-2026-10-03/device-defaults.json'));
const old=JSON.parse(fs.readFileSync('assets/pattern-catalog.json'));
if(JSON.stringify(old)!==JSON.stringify(snapshot.initial.catalog)||old.length!==49)throw Error('Catalog changed; do not replay migration');
const gone=snapshot.patterns.filter(x=>x.bookmarked).map(x=>x.variant);
if(JSON.stringify(gone)!=='[26,34,37,39,40,41,44]')throw Error('Unexpected deletion targets');
const added=[
 ['Bioluminescent Topo Vortex — Abyssal',['Ring count','Flow speed','Vortex twist','Braiding'],[50,23,44,50],[0x00FF2D,0x00DCD7,0x0064FF,0x001CC3],'@martinnlopez · advanced variation'],
 ['Warped Checkerboard — Interference',['Motion speed','Wavelength','Distortion','Wave interference'],[28,24,41,55],[0xFF3155,0xFFC247,0x22D3C5,0x765DFF],'Built-in C++ · advanced variation'],
 ['Ripples',['Expansion speed','Ring width','Spawn density','Decay'],[64,24,55,24],[0x36D5B6,0x3EC1DE,0x4166FF,0x9654F5],'User-supplied Arduino algorithm']
];
let ino=fs.readFileSync('64x64_matrix_01.ino','utf8');
const meta=JSON.parse(ino.match(/const pfMeta=(.*);/)[1]),authors=JSON.parse(ino.match(/const pfAuthors=(.*);/)[1]);
meta[43][1][3]='Line width';meta[45][1][1]='Particle count';
const catalog=old.filter(x=>!gone.includes(x.variant));
for(let i=0;i<added.length;i++){const [name,labels,,,author]=added[i];meta.push([name,labels]);authors.push(author);catalog.push({name,engine:16,variant:46+i});}
const active=catalog.filter(x=>x.engine===16).map(x=>x.variant);
ino=ino.replace(/const pfMeta=.*;/,'const pfMeta='+JSON.stringify(meta)+';')
 .replace(/const pfAuthors=.*;/,'const pfAuthors='+JSON.stringify(authors)+';')
 .replace(/const collectionPatternIds=.*;/,'const collectionPatternIds='+JSON.stringify(active)+';')
 .replace(/const uint8_t patternflowRenderSize\[PATTERNFLOW_PATTERN_COUNT\] = \{([^}]+)\}/,(_,sizes)=>'const uint8_t patternflowRenderSize[PATTERNFLOW_PATTERN_COUNT] = {'+sizes+',32,32,32}')
 .replace('>49 patterns<','>45 patterns<').replace('state.catalog?.length||49','state.catalog?.length||45').replace("'whitney-49-24'","'advanced-45-24'");
fs.writeFileSync('64x64_matrix_01.ino',ino);
fs.writeFileSync('assets/pattern-catalog.json',JSON.stringify(catalog,null,2)+'\n');
let header=fs.readFileSync('src/generated/PatternCatalog.h','utf8');
header=header.replace('COLLECTION_PATTERN_COUNT=39','COLLECTION_PATTERN_COUNT='+active.length)
 .replace(/COLLECTION_PATTERN_IDS\[COLLECTION_PATTERN_COUNT\]=\{[^}]+\}/,'COLLECTION_PATTERN_IDS[COLLECTION_PATTERN_COUNT]={'+active+'}');
fs.writeFileSync('src/generated/PatternCatalog.h',header);
let cpp=fs.readFileSync('src/patterns/PatternflowPack.cpp','utf8').replace('"Pause duration"','"Line width"').replace('"Tile count"','"Particle count"');
const entries=added.map(([name,labels,defaults,colors])=>'  {'+JSON.stringify(name)+', {'+labels.map(x=>JSON.stringify(x)).join(',')+'}, {'+defaults+'}, {'+colors.map(c=>'0x'+c.toString(16).padStart(6,'0')).join(',')+'}},').join('\n');
cpp=cpp.replace('\n};\n\nnamespace {','\n'+entries+'\n};\n\nnamespace {');fs.writeFileSync('src/patterns/PatternflowPack.cpp',cpp);
// Remove renderer bodies; do not renumber persistent metadata/patch slots.
function removeCases(file,ids){let s=fs.readFileSync(file,'utf8');for(const id of ids){
 const start=new RegExp('^([ \\t]*)case '+id+':\\{','m').exec(s);if(!start)throw Error('Missing case '+id+' in '+file);
 const rest=s.slice(start.index+start[0].length),next=/^\s*case \d+:|^  \}\n\}|^ \}\n for\(int y=0/m.exec(rest);
 if(!next)throw Error('Missing case boundary');s=s.slice(0,start.index)+rest.slice(next.index);
 }fs.writeFileSync(file,s);}
removeCases('src/patterns/PatternflowImports.inc',[9,17]);
removeCases('src/patterns/PatternRemixes.inc',[2,4,5,6]);
removeCases('src/patterns/WhitneyStudies.inc',[1]);
console.log('Removed exactly 7 archived bookmarks; added 3 patterns; 45 active entries.');
