const fs=require('fs'),path=require('path');process.chdir(path.resolve(__dirname,'..'));
let ino=fs.readFileSync('64x64_matrix_01.ino','utf8');
const meta=JSON.parse(ino.match(/const pfMeta=(.*);/)[1]),authors=JSON.parse(ino.match(/const pfAuthors=(.*);/)[1]),ids=JSON.parse(ino.match(/const collectionPatternIds=(.*);/)[1]);
const catalog=JSON.parse(fs.readFileSync('assets/pattern-catalog.json'));
if(meta.length!==58||catalog.length!==54)throw Error('Expected 54 patterns; do not repeat');
const name='Pong Wars',labels=['Game speed','Tile size','Ball size','Bounce variation'];
meta.push([name,labels]);authors.push('Koen van Gilst · MIT · native adaptation');ids.push(58);catalog.push({name,engine:16,variant:58});
ino=ino.replace(/const pfMeta=.*;/,'const pfMeta='+JSON.stringify(meta)+';').replace(/const pfAuthors=.*;/,'const pfAuthors='+JSON.stringify(authors)+';')
 .replace(/const collectionPatternIds=.*;/,'const collectionPatternIds='+JSON.stringify(ids)+';')
 .replace(/const uint8_t patternflowRenderSize\[PATTERNFLOW_PATTERN_COUNT\] = \{([^}]+)\}/,(_,s)=>'const uint8_t patternflowRenderSize[PATTERNFLOW_PATTERN_COUNT] = {'+s+',32}')
 .replace('>54 patterns<','>55 patterns<').replace('state.catalog?.length||54','state.catalog?.length||55').replace("'nova-photos-54-24'","'pong-55-24'");
fs.writeFileSync('64x64_matrix_01.ino',ino);fs.writeFileSync('assets/pattern-catalog.json',JSON.stringify(catalog,null,2)+'\n');
let h=fs.readFileSync('src/generated/PatternCatalog.h','utf8').replace('COLLECTION_PATTERN_COUNT=44','COLLECTION_PATTERN_COUNT=45').replace(/COLLECTION_PATTERN_IDS\[COLLECTION_PATTERN_COUNT\]=\{[^}]+\}/,'COLLECTION_PATTERN_IDS[COLLECTION_PATTERN_COUNT]={'+ids+'}');fs.writeFileSync('src/generated/PatternCatalog.h',h);
let p=fs.readFileSync('src/patterns/PatternflowPack.h','utf8').replace('PATTERNFLOW_PATTERN_COUNT = 58','PATTERNFLOW_PATTERN_COUNT = 59');fs.writeFileSync('src/patterns/PatternflowPack.h',p);
let cpp=fs.readFileSync('src/patterns/PatternflowPack.cpp','utf8');
cpp=cpp.replace('\n};\n\nnamespace {','\n  {"Pong Wars", {"Game speed","Tile size","Ball size","Bounce variation"}, {40,33,50,25}, {0xD9E8E3,0xD9E8E3,0x172B36,0x172B36}},\n};\n\nnamespace {');fs.writeFileSync('src/patterns/PatternflowPack.cpp',cpp);
console.log('Pong Wars appended; 55 entries, no bookmark/default migration.');
