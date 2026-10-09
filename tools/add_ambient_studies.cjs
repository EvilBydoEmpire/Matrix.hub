// One-time mechanical registration; stable IDs append, never reorder old rows.
const fs=require('fs');
const file='src/patterns/PatternflowPack.cpp';let s=fs.readFileSync(file,'utf8');
if(s.includes('Strobe — Chrono Iris'))throw Error('Already applied');
const added=[
 ['Strobe — Chrono Iris',['Pulse rate','Petal count','Exposure width','Flash depth'],[8,40,60,18],[0x5171A6,0xA8C8D6,0xE1D9BA,0xA8C8D6]],
 ['Strobe — Rolling Silk',['Pulse rate','Curtain density','Exposure width','Flash depth'],[6,32,65,16],[0x3B7380,0x9AC4BB,0xDDCFA5,0x9AC4BB]],
 ['Strobe — Phase Garden',['Pulse rate','Lantern count','Exposure width','Flash depth'],[9,35,55,20],[0xAF704E,0xE0BB83,0xAF704E,0xE0BB83]]
];
const rows=added.map(([n,l,d,c])=>`  {${JSON.stringify(n)}, {${l.map(JSON.stringify).join(',')}}, {${d}}, {${c.map(v=>'0x'+v.toString(16))}}},`).join('\n');
s=s.replace('\n};\n\nnamespace {','\n'+rows+'\n};\n\nnamespace {');
s=s.replace('#include "Classic08.inc"','#include "Classic08.inc"\n#include "AmbientStrobes.inc"');
s=s.replace('  resetClassic08();','  resetClassic08();\n  resetAmbientStrobes();');
s=s.replace('  if(index >= 63)', '  if(index >= 65) { renderAmbientStrobe(index-65,width,height,deltaTime,p,palette,writer,context); return; }\n  if(index >= 63)');
// Early generic adaptations had the same placeholder spectrum, not source art.
const curated={0:[0x174D62,0x3DADAE,0xC4E3D4,0x3DADAE],1:[0x283B8C,0x4ACAC8,0xD3B2DE,0x4ACAC8],2:[0x103B59,0x459EB2,0xCAE1D6,0x459EB2],3:[0x622E75,0xC88098,0xECD5AA,0xC88098],4:[0x243F56,0x92B7BA,0xD9AD7E,0x92B7BA],5:[0xB05857,0xE8B987,0x468F9B,0xE8B987],6:[0xDFDDD1,0xDFDDD1,0xDFDDD1,0xDFDDD1],7:[0x337A89,0xC0DCD1,0x337A89,0xC0DCD1],11:[0xBDCFC7,0xBDCFC7,0xBDCFC7,0xBDCFC7],12:[0x8EB8CD,0xE7D5AA,0x8EB8CD,0xE7D5AA],13:[0x17535E,0x4FBAA9,0xC7DDD0,0x4FBAA9],14:[0x355883,0xB96858,0xE4C784,0x559E96],15:[0x183E55,0x388C95,0xC5DCC2,0x388C95],16:[0xE3D4B5,0xAE6C89,0xE3D4B5,0x70ADB9]};
let lines=s.split('\n'),idx=0;lines=lines.map(line=>{if(!/^  \{"/.test(line))return line;const colors=curated[idx++];return colors?line.replace(/\{0x[\da-fA-F]+(?:,0x[\da-fA-F]+){3}\}/,'{'+colors.map(v=>'0x'+v.toString(16)).join(',')+'}'):line});s=lines.join('\n');fs.writeFileSync(file,s);
const h='src/patterns/PatternflowPack.h';fs.writeFileSync(h,fs.readFileSync(h,'utf8').replace('COUNT = 65','COUNT = 68'));
const cat='src/generated/PatternCatalog.h';fs.writeFileSync(cat,fs.readFileSync(cat,'utf8').replace('COUNT=49','COUNT=52').replace('62,63,64};','62,63,64,65,66,67};'));
let ino=fs.readFileSync('64x64_matrix_01.ino','utf8').replace('#include "src/patterns/SoukanPalettes.h"','#include "src/patterns/SoukanPalettes.h"\n#include "src/patterns/ArtDirection.h"').replaceAll('applySoukanDefault','applyArtDefault').replaceAll('saveSoukanOverride','saveArtOverride');
for(const [name,append] of [['pfMeta',added.map(x=>[x[0],x[1]])],['pfAuthors',added.map(_=>'Televator · Ambient studies')],['collectionPatternIds',[65,66,67]]]){
 const re=new RegExp('const '+name+'=(\\[.*?\\]);');const m=ino.match(re);if(!m)throw Error(name);ino=ino.replace(re,'const '+name+'='+JSON.stringify(JSON.parse(m[1]).concat(append))+';');
}
ino=ino.replaceAll('59 patterns','62 patterns').replace("'Original','Ember'","'Authored / curated','Ember'");
ino=ino.replace("async function selectGalleryPattern(item){", "async function selectGalleryPattern(item){if(item.engine===16&&item.variant>=65&&!confirm('Temporal light study: flashing can cause discomfort or seizures in susceptible viewers. Starts with gentle modulation; raise Flash depth only deliberately. These studies never auto-cycle. Continue?'))return;");
fs.writeFileSync('64x64_matrix_01.ino',ino);
const catalog=JSON.parse(fs.readFileSync('assets/pattern-catalog.json'));console.log(catalog[0]);
added.forEach((x,i)=>catalog.push({name:x[0],engine:16,variant:65+i}));fs.writeFileSync('assets/pattern-catalog.json',JSON.stringify(catalog,null,2)+'\n');
const soukan='src/patterns/SoukanPalettes.h';s=fs.readFileSync(soukan,'utf8').replace('Variants[65]','Variants[68]').replace('10,5,6\n','10,5,6,3,4,7\n');fs.writeFileSync(soukan,s);
