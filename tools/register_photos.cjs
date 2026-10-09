const fs=require('fs'),path=require('path');process.chdir(path.resolve(__dirname,'..'));
let ino=fs.readFileSync('64x64_matrix_01.ino','utf8');
const meta=JSON.parse(ino.match(/const pfMeta=(.*);/)[1]),authors=JSON.parse(ino.match(/const pfAuthors=(.*);/)[1]),ids=JSON.parse(ino.match(/const collectionPatternIds=(.*);/)[1]);
const catalog=JSON.parse(fs.readFileSync('assets/pattern-catalog.json'));
if(meta.length!==56||catalog.length!==52)throw Error('Expected Nova catalog; do not replay');
const labels=['Exposure','Contrast','Black level','Color temperature'];
for(let i=0;i<2;i++){let name='Foto — IMG_'+[4786,4704][i];meta.push([name,labels]);authors.push('Personal photo · local/device only');ids.push(56+i);catalog.push({name,engine:16,variant:56+i});}
ino=ino.replace(/const pfMeta=.*;/,'const pfMeta='+JSON.stringify(meta)+';').replace(/const pfAuthors=.*;/,'const pfAuthors='+JSON.stringify(authors)+';')
 .replace(/const collectionPatternIds=.*;/,'const collectionPatternIds='+JSON.stringify(ids)+';')
 .replace(/const uint8_t patternflowRenderSize\[PATTERNFLOW_PATTERN_COUNT\] = \{([^}]+)\}/,(_,s)=>'const uint8_t patternflowRenderSize[PATTERNFLOW_PATTERN_COUNT] = {'+s+',32,32}')
 .replace('>52 patterns<','>54 patterns<').replace('state.catalog?.length||52','state.catalog?.length||54').replace("'nova-52-24'","'nova-photos-54-24'");
fs.writeFileSync('64x64_matrix_01.ino',ino);fs.writeFileSync('assets/pattern-catalog.json',JSON.stringify(catalog,null,2)+'\n');
let h=fs.readFileSync('src/generated/PatternCatalog.h','utf8').replace('COLLECTION_PATTERN_COUNT=42','COLLECTION_PATTERN_COUNT=44').replace(/COLLECTION_PATTERN_IDS\[COLLECTION_PATTERN_COUNT\]=\{[^}]+\}/,'COLLECTION_PATTERN_IDS[COLLECTION_PATTERN_COUNT]={'+ids+'}');fs.writeFileSync('src/generated/PatternCatalog.h',h);
let p=fs.readFileSync('src/patterns/PatternflowPack.h','utf8').replace('PATTERNFLOW_PATTERN_COUNT = 56','PATTERNFLOW_PATTERN_COUNT = 58');fs.writeFileSync('src/patterns/PatternflowPack.h',p);
let cpp=fs.readFileSync('src/patterns/PatternflowPack.cpp','utf8');
const colors=[[0xbca18d,0x404d61,0x909a9c,0xede8dc],[0x365c47,0x939d8c,0xe2b79a,0x444955]];
let entries=[0,1].map(i=>'  {'+JSON.stringify(meta[56+i][0])+', {'+labels.map(s=>JSON.stringify(s)).join(',')+'}, {50,50,50,50}, {'+colors[i].map(c=>'0x'+c.toString(16)).join(',')+'}},').join('\n');
cpp=cpp.replace('\n};\n\nnamespace {','\n'+entries+'\n};\n\nnamespace {');fs.writeFileSync('src/patterns/PatternflowPack.cpp',cpp);
console.log('Two personal photo patterns appended; 54 active patterns. No publication.');
