// One-shot, snapshot-checked revision 2 migration. Never silently repeat it.
const fs=require('fs'),path=require('path');
const root=path.resolve(__dirname,'..');process.chdir(root);
const snapshot=JSON.parse(fs.readFileSync('/private/tmp/matrix-before-remix.json','utf8'));
const marked=snapshot.catalog.filter((_,i)=>snapshot.bookmarks[i]);
const expected=['Floyd Flow','Velvet Steps','TV Setup','Pareidolia','Midnight Metropolis','Mercury Rain','Entropy Glass','Bus Window Rain'];
if(JSON.stringify(marked.map(x=>x.name))!==JSON.stringify(expected))throw Error('Bookmarks changed: inspect before deleting');
const archive='patterns/archive/pre-remix-2026-10-03';
if(fs.existsSync(archive))throw Error('Migration archive already exists; do not repeat');
fs.mkdirSync(archive,{recursive:true});
for(const file of ['64x64_matrix_01.ino','src/patterns/PatternflowPack.cpp','src/patterns/PatternflowPack.h','src/patterns/PatternflowImports.inc','src/patterns/LibrarySettings.inc','src/generated/PatternCatalog.h','assets/pattern-catalog.json','assets/pattern-preview-atlas.png']){
 fs.copyFileSync(file,path.join(archive,path.basename(file)));
}
fs.writeFileSync(path.join(archive,'selection.json'),JSON.stringify({catalog:snapshot.catalog,bookmarks:snapshot.bookmarks,removed:marked,params:snapshot.params,currentPattern:snapshot.currentPattern},null,2)+'\n');
const gone=new Set([17,20,24,27,28,30,32]);
const core=[0,1,3,5,6,7,9,11,12,14];
const active=Array.from({length:43},(_,i)=>i).filter(i=>!gone.has(i));
const remixes=[
 ['Whitney Harmonics II',['Canon speed','Voices','Orbit size','Trail length'],[20,58,66,58],[0x16BFFD,0x51FFC5,0xFFD98B,0xC748FF],'Imported Arduino sketch · remix'],
 ['Chladni Resonance',['Mode N','Mode M','Morph speed','Filament width'],[30,57,26,40],[0x26207E,0x914FFF,0xFF6093,0xFFCD91],'@engmung · remix'],
 ['Vega444 C — Orbital',['Grid density','Lens bulge','Breathing depth','Orbit speed'],[45,62,58,25],[0xEB4149,0xFFBB69,0x35DCAA,0x528AFF],'@WollyMeister · remix'],
 ['Warped Checkerboard — Silk',['Flow speed','Cell size','Warp strength','Fabric fold'],[26,47,65,52],[0x5740CB,0xEF62CA,0xFFC09A,0x51DACE],'Built-in C++ · remix'],
 ['Bioluminescent Topo Vortex II',['Ring count','Flow speed','Vortex twist','Strand beads'],[50,27,60,45],[0x3EFF88,0x22E6C8,0x3885FF,0x7829D6],'@martinnlopez · remix'],
 ['Ripple Cell Grid 2 — Liquid',['Wave speed','Cell size','Ripple depth','Bubble size'],[33,50,65,51],[0xFF3E56,0xFFB647,0xFFF19C,0x43D8B7],'@engmung · remix'],
 ['Twisted Tunnel — Hyperspace',['Travel speed','Portal density','Twist','Flight path'],[25,53,64,42],[0xFF345E,0xFFBE4F,0x21CCC4,0x5D48DF],'@kpow · remix'],
 ['Squares — Infinite',['Travel speed','Square count','Corkscrew','Line weight'],[28,48,46,34],[0x29D8FF,0x745AFF,0xFF50BB,0xFFBD59],'Built-in C++ · remix']
];
let ino=fs.readFileSync('64x64_matrix_01.ino','utf8');
const meta=JSON.parse(ino.match(/const pfMeta=(.*);/)[1]);
const authors=JSON.parse(ino.match(/const pfAuthors=(.*);/)[1]);
for(const [name,labels,,,author] of remixes){meta.push([name,labels]);authors.push(author);}
ino=ino.replace(/const pfMeta=.*;/,'const pfMeta='+JSON.stringify(meta)+';')
 .replace(/const pfAuthors=.*;/,'const pfAuthors='+JSON.stringify(authors)+';')
 .replace(/const corePatternIds=.*;/,'const corePatternIds='+JSON.stringify(core)+';\nconst collectionPatternIds='+JSON.stringify(active)+';')
 .replace('...pfMeta.map((m,i)=>({key:', '...collectionPatternIds.map(i=>({key:').replace('name:m[0],engine:16','name:pfMeta[i][0],engine:16')
 .replace('patternBookmarks[CORE_PATTERN_COUNT + PATTERNFLOW_PATTERN_COUNT]','patternBookmarks[GALLERY_PATTERN_COUNT]')
 .replace('int v=next-CORE_PATTERN_COUNT;','int v=COLLECTION_PATTERN_IDS[next-CORE_PATTERN_COUNT];')
 .replace('for(int v=0;v<PATTERNFLOW_PATTERN_COUNT;v++){JsonObject item=catalog.createNestedObject();','for(int j=0;j<COLLECTION_PATTERN_COUNT;j++){int v=COLLECTION_PATTERN_IDS[j];JsonObject item=catalog.createNestedObject();')
 .replace('if (variant < 0 || variant >= PATTERNFLOW_PATTERN_COUNT) success = false;','if (!isAvailableVariant(variant)) success = false;')
 .replace(/const uint8_t patternflowRenderSize\[PATTERNFLOW_PATTERN_COUNT\] = \{([^}]+)\}/,(_,s)=>'const uint8_t patternflowRenderSize[PATTERNFLOW_PATTERN_COUNT] = {'+s+',32,32,32,32,32,32,32,32}');
// Remove only the renderer case, not stable native NVS patch slots.
ino=ino.replace(/    case FLOYD_FLOW: \{[\s\S]*?(?=    case SNAKE: \{)/,'');
fs.writeFileSync('64x64_matrix_01.ino',ino);
fs.writeFileSync('src/generated/PatternCatalog.h',`#pragma once
// Stable engine/variant identities; active gallery order is intentionally dense.
constexpr uint8_t CORE_PATTERN_COUNT=${core.length};
constexpr uint8_t COLLECTION_PATTERN_COUNT=${active.length};
constexpr uint8_t GALLERY_PATTERN_COUNT=CORE_PATTERN_COUNT+COLLECTION_PATTERN_COUNT;
const uint8_t CORE_PATTERN_IDS[CORE_PATTERN_COUNT]={${core}};
const uint8_t COLLECTION_PATTERN_IDS[COLLECTION_PATTERN_COUNT]={${active}};
const uint8_t LEGACY_VARIANT_IDS[17]={0,1,8,9,13,14,17,18,20,21,23,27,31,32,35,36,37};
`);
const catalog=snapshot.catalog.filter((_,i)=>!snapshot.bookmarks[i]);
remixes.forEach((r,i)=>catalog.push({name:r[0],engine:16,variant:35+i}));
fs.writeFileSync('assets/pattern-catalog.json',JSON.stringify(catalog,null,2)+'\n');
let cpp=fs.readFileSync('src/patterns/PatternflowPack.cpp','utf8');
const entries=remixes.map(([name,labels,defaults,colors])=>'  {'+JSON.stringify(name)+', {'+labels.map(s=>JSON.stringify(s)).join(',')+'}, {'+defaults+'}, {'+colors.map(c=>'0x'+c.toString(16).padStart(6,'0')).join(',')+'}},').join('\n');
cpp=cpp.replace('\n};\n\nnamespace {','\n'+entries+'\n};\n\nnamespace {');
fs.writeFileSync('src/patterns/PatternflowPack.cpp',cpp);
let imports=fs.readFileSync('src/patterns/PatternflowImports.inc','utf8');
for(const relative of [0,3,7,10,11,13,15]){
 imports=imports.replace(new RegExp('    case '+relative+':\\{[\\s\\S]*?(?=    case [0-9]+:|  \\}\\n\\})'),'');
}
imports=imports.replaceAll('index==4||index==6||index==7||index==11||index==13||index==14||index==15||index==17','index==4||index==6||index==14||index==17');
fs.writeFileSync('src/patterns/PatternflowImports.inc',imports);
console.log('Archived and removed 8 bookmarked patterns; retained 38 originals; registered 8 remixes.');
