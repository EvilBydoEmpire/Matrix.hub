// Append only. Retired variant IDs and all existing presets remain untouched.
const fs=require('fs'),path=require('path');process.chdir(path.resolve(__dirname,'..'));
let ino=fs.readFileSync('64x64_matrix_01.ino','utf8');
const meta=JSON.parse(ino.match(/const pfMeta=(.*);/)[1]),authors=JSON.parse(ino.match(/const pfAuthors=(.*);/)[1]),ids=JSON.parse(ino.match(/const collectionPatternIds=(.*);/)[1]);
const catalog=JSON.parse(fs.readFileSync('assets/pattern-catalog.json'));
if(meta.length!==63||catalog.length!==59)throw Error('Expected pre-import catalog; do not repeat');
for(const [i,name,labels] of [[63,'Ripples — Classic 08',['Drop frequency','Trail fade','Ring width','Wave speed']],[64,'Plasma — Classic 08',['Flow speed','Wave scale','Hue drift','Trail fade']]]){
 meta.push([name,labels]);authors.push('User sketch · LED_Matrix_08');ids.push(i);catalog.push({name,engine:16,variant:i});
}
ino=ino.replace(/const pfMeta=.*;/,'const pfMeta='+JSON.stringify(meta)+';').replace(/const pfAuthors=.*;/,'const pfAuthors='+JSON.stringify(authors)+';')
 .replace(/const collectionPatternIds=.*;/,'const collectionPatternIds='+JSON.stringify(ids)+';')
 .replace(/const uint8_t patternflowRenderSize\[PATTERNFLOW_PATTERN_COUNT\] = \{([^}]+)\}/,(_,s)=>'const uint8_t patternflowRenderSize[PATTERNFLOW_PATTERN_COUNT] = {'+s+',64,64}')
 .replace('>59 patterns<','>61 patterns<').replace('state.catalog?.length||59','state.catalog?.length||61');
fs.writeFileSync('64x64_matrix_01.ino',ino);fs.writeFileSync('assets/pattern-catalog.json',JSON.stringify(catalog,null,2)+'\n');
console.log('Added Classic 08 variants 63/64; gallery 61 patterns.');
