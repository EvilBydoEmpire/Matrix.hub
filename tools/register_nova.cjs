// Append-only addition; never reset/remap existing bookmarks or preset IDs.
const fs=require('fs'),path=require('path');process.chdir(path.resolve(__dirname,'..'));
const additions=[
 ['Luminous Loom',['Weave speed','Ribbon count','Undulation','Ribbon width'],[28,40,55,65],[0xff6a65,0xffc983,0x23d8c6,0x5856de]],
 ['Ferrofluid',['Flow speed','Droplets','Cohesion','Metallic reflection'],[25,50,36,85],[0x173c67,0x43d9cb,0xeaf1dd,0xb854e7]],
 ['Accretion',['Orbital speed','Disc tilt','Event horizon','Turbulence'],[30,34,45,55],[0xffdf91,0xffa044,0xe3426c,0x7050d4]],
 ['Origami Flight',['Motion speed','Facet count','Folding depth','Precession'],[28,35,66,55],[0xffbc85,0xe96685,0x56bdc6,0xb3b2ed]],
 ['Signal Orchard',['Pulse speed','Trees','Branching depth','Surge'],[32,20,40,70],[0x22f6bc,0x3cc7e8,0x7874ff,0xe957b8]],
 ['Whitney — Resonant Orbits',['Canon speed','Particle count','Rate divergence','Echo length'],[26,50,75,40],[0xf4e2b2,0xe18680,0x48c8c1,0x868ee5]],
 ['Whitney — Phase Architecture',['Motion speed','Strand count','Torsion','Precession'],[26,45,65,55],[0xefe4c9,0xd782a6,0x74c9d0,0x9799ed]]
];
let ino=fs.readFileSync('64x64_matrix_01.ino','utf8');
const meta=JSON.parse(ino.match(/const pfMeta=(.*);/)[1]),authors=JSON.parse(ino.match(/const pfAuthors=(.*);/)[1]);
const ids=JSON.parse(ino.match(/const collectionPatternIds=(.*);/)[1]);
const catalog=JSON.parse(fs.readFileSync('assets/pattern-catalog.json'));
if(meta.length!==49||catalog.length!==45)throw Error('Catalog changed; do not repeat registration');
for(let i=0;i<additions.length;i++){const [name,labels]=additions[i];meta.push([name,labels]);authors.push(i>=5?'Original C++ · John Whitney inspiration':'Original C++ · Nova studies');ids.push(49+i);catalog.push({name,engine:16,variant:49+i});}
ino=ino.replace(/const pfMeta=.*;/,'const pfMeta='+JSON.stringify(meta)+';').replace(/const pfAuthors=.*;/,'const pfAuthors='+JSON.stringify(authors)+';')
 .replace(/const collectionPatternIds=.*;/,'const collectionPatternIds='+JSON.stringify(ids)+';')
 .replace(/const uint8_t patternflowRenderSize\[PATTERNFLOW_PATTERN_COUNT\] = \{([^}]+)\}/,(_,s)=>'const uint8_t patternflowRenderSize[PATTERNFLOW_PATTERN_COUNT] = {'+s+',32,32,32,32,32,32,32}')
 .replace('>45 patterns<','>52 patterns<').replace('state.catalog?.length||45','state.catalog?.length||52').replace("'advanced-45-24'","'nova-52-24'");
fs.writeFileSync('64x64_matrix_01.ino',ino);fs.writeFileSync('assets/pattern-catalog.json',JSON.stringify(catalog,null,2)+'\n');
let header=fs.readFileSync('src/generated/PatternCatalog.h','utf8').replace('COLLECTION_PATTERN_COUNT=35','COLLECTION_PATTERN_COUNT=42').replace(/COLLECTION_PATTERN_IDS\[COLLECTION_PATTERN_COUNT\]=\{[^}]+\}/,'COLLECTION_PATTERN_IDS[COLLECTION_PATTERN_COUNT]={'+ids+'}');
fs.writeFileSync('src/generated/PatternCatalog.h',header);
let pack=fs.readFileSync('src/patterns/PatternflowPack.h','utf8').replace('PATTERNFLOW_PATTERN_COUNT = 49','PATTERNFLOW_PATTERN_COUNT = 56');fs.writeFileSync('src/patterns/PatternflowPack.h',pack);
let cpp=fs.readFileSync('src/patterns/PatternflowPack.cpp','utf8');
let entries=additions.map(([name,labels,defaults,colors])=>'  {'+JSON.stringify(name)+', {'+labels.map(x=>JSON.stringify(x)).join(',')+'}, {'+defaults+'}, {'+colors.map(c=>'0x'+c.toString(16).padStart(6,'0')).join(',')+'}},').join('\n');
cpp=cpp.replace('\n};\n\nnamespace {','\n'+entries+'\n};\n\nnamespace {');fs.writeFileSync('src/patterns/PatternflowPack.cpp',cpp);
console.log('Registered seven original studies; 52 active patterns, existing IDs/bookmarks preserved.');
