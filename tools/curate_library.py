#!/usr/bin/env python3
"""One-time, audited mechanical curation of the October 3 bookmark snapshot.

Archives the previous renderer before removing cases. Never queries credentials.
"""
import json
import pathlib
import re
import shutil

ROOT = pathlib.Path(__file__).resolve().parent.parent
KEEP_CORE = [0,1,3,5,6,7,9,10,11,12,14]
KEEP_VARIANTS = [0,1,8,9,13,14,17,18,20,21,23,27,31,32,35,36,37]
NEW = [
 ('velvet-steps','Velvet Steps',['Walk','Direction','Cat scale','Frost'],[30,0,42,25],['EBECE9','595C5F','BA7B8B','E6BEC3']),
 ('vega444-c','Vega444 C',['Grid','Bulge','Wave depth','Color drift'],[67,75,47,0],['D92929','24A361','576FD6','1C7052']),
 ('twisted-tunnel','Twisted Tunnel',['Ring spacing','Twist','Accent hue','Travel speed'],[50,50,50,8],['D11414','0F478C','F2C20A','FFFFFF']),
 ('tv-setup','TV Setup',['Channel tuning','Interference','Snow','RGB convergence'],[0,12,0,50],['FFFFFF','FFFF00','00FFFF','FF00FF']),
 ('solitonlatticeecho','Soliton Lattice Echo',['Kink energy','Relative pace','Soliton count','Echo decay'],[10,10,50,83],['000000','363636','737373','A1A1A1']),
 ('ripplecellgrid2','Ripple Cell Grid 2',['Wave speed','Cell size','Ripple depth','Dot width'],[50,67,50,40],['000000','FF0000','EEFF00','000000']),
 ('poolside','Poolside',['Pool depth','Wave drift','Chop','Swell wavelength'],[33,26,50,0],['000000','1100FF','FF0000','FF0000']),
 ('pareidolia','Pareidolia',['Drift speed','Noise detail','Ink density','Domain warp'],[19,50,50,48],['212865','000000','00F900','000000']),
 ('op-art-squircle-spacetime-warp','Squircle Spacetime Warp',['Circle to square','Gravity lens','Grid frequency','Cell phase'],[65,44,29,0],['000000','55555B','AAAAB0','FFFFFF']),
 ('mondrian','Mondrian',['Accent hue','Hold time','Cell count','Move speed'],[50,35,27,32],['FFFAEA','D11414','0F478C','F2C20A']),
 ('midnight-metropolis','Midnight Metropolis',['Scroll speed','City mood','Haze','Window activity'],[29,45,50,39],['10143E','FEF032','FF3C5F','00EBD7']),
 ('mercury-rain','Mercury Rain',['Rain rate','Ripple size','Expansion speed','Chrome contrast'],[26,35,35,29],['000000','555555','AAAAAA','FFFFFF']),
 ('hypno-circles','Hypno Circles',['Spectrum center','Motion speed','Wave ratio','White intersections'],[0,19,31,50],['FF0000','FFFF00','00FFFF','FF00FF']),
 ('entropy-glass','Entropy Glass',['Neck width','Sorting demon','Gravity','Color blindness'],[0,100,0,0],['FFF0CD','FF4E34','AA3CFF','00CDBE']),
 ('collatz-kelp','Collatz Kelp',['Odd turn angle','Growth speed','Even turn angle','Seed count'],[0,36,0,0],['0A7882','6EEB78','FF6E50','FFCD46']),
 ('bus-window-rain','Bus Window Rain',['Rain rate','Fog regrowth','Bus speed','Light focus'],[30,30,20,0],['FF9628','FF1EAA','00E1FF','E1EBFF']),
 ('bioluminescent-topo-vortex','Bioluminescent Topo Vortex',['Ring count','Rotation speed','Twist','Color bias'],[50,23,44,50],['00FF2D','00DCD7','0064FF','001CC3']),
 ('aizawaviscousinkplume','Aizawa Viscous Ink Plume',['Viscosity bifurcation','Downflow','Vortex core','Plume thickness'],[71,19,57,44],['000000','6A007A','E100FF','FFFFFF']),
]

def main():
    raise SystemExit('Completed one-time migration. Do not rerun against the curated sources; see patterns/archive/pre-curation-2026-10-03.')
    cpp_path=ROOT/'src/patterns/PatternflowPack.cpp'
    ino_path=ROOT/'64x64_matrix_01.ino'
    archive=ROOT/'patterns/archive/pre-curation-2026-10-03'
    archive.mkdir(parents=True,exist_ok=True)
    for path in [cpp_path,ROOT/'src/patterns/PatternflowPack.h',ino_path]:
        dest=archive/path.name
        if not dest.exists(): shutil.copyfile(path,dest)
    cpp=(archive/cpp_path.name).read_text(); ino=(archive/ino_path.name).read_text()
    meta_start=cpp.index('const PatternflowMetadata ')
    meta_end=cpp.index('\n};',meta_start)+3
    old_meta=re.findall(r'^  \{".*$',cpp[meta_start:meta_end],re.M)
    assert len(old_meta)==42
    snapshot={'date':'2026-10-03','stateRevision':196,'retainedOriginalGalleryIndices':KEEP_CORE+[16+i for i in KEEP_VARIANTS],
              'removedOriginalGalleryIndices':[i for i in range(58) if i not in KEEP_CORE+[16+i for i in KEEP_VARIANTS]],
              'bookmarksAfterMigration':False,'archive':'patterns/archive/pre-curation-2026-10-03'}
    (archive/'bookmark-snapshot.json').write_text(json.dumps(snapshot,indent=2)+'\n')
    # Keep original case bodies byte-for-byte; only indices change.
    matches=list(re.finditer(r'^    case (\d+): \{',cpp,re.M))
    end=cpp.rindex('\n  }')
    blocks={int(m[1]):cpp[m.start():(matches[j+1].start() if j+1<len(matches) else end)] for j,m in enumerate(matches)}
    retained=''.join(re.sub(r'case \d+:',f'case {n}:',blocks[i],count=1) for n,i in enumerate(KEEP_VARIANTS))
    cpp=cpp[:matches[0].start()]+retained+'    default: break;'+cpp[end:]
    # Removed raymarcher/flock fields are no longer referenced.
    a=cpp.index('  if (index == 24)'); b=cpp.index('\n}',a)
    cpp=cpp[:a]+'  resetImportedPattern();'+cpp[b:]
    old_colors=['FF3155','FFC247','22D3C5','765DFF']
    lines=[old_meta[i].rstrip(',')[:-1]+', {'+','.join('0x'+c for c in old_colors)+'}},' for i in KEEP_VARIANTS]
    for slug,name,labels,defaults,colors in NEW:
        lines.append('  {'+json.dumps(name)+', {'+','.join(json.dumps(x) for x in labels)+'}, {'+','.join(map(str,defaults))+'}, {'+','.join('0x'+c for c in colors)+'}},')
    a=cpp.index('const PatternflowMetadata '); b=cpp.index('\n};',a)+3
    cpp=cpp[:a]+'const PatternflowMetadata PATTERNFLOW_METADATA[PATTERNFLOW_PATTERN_COUNT] = {\n'+'\n'.join(lines)+'\n};'+cpp[b:]
    cpp=cpp.replace('\nvoid PatternflowPack::reset','\n#include "PatternflowImports.inc"\n\nvoid PatternflowPack::reset',1)
    cpp=cpp.replace('  float& t = time_[index];','  if(index >= 17) { renderImportedPattern(index-17,width,height,deltaTime,p,palette,writer,context); return; }\n  float& t = time_[index];',1)
    cpp_path.write_text(cpp)
    h=ROOT/'src/patterns/PatternflowPack.h'
    header=(archive/h.name).read_text().replace('COUNT = 42','COUNT = 35').replace('uint8_t defaults[4];','uint8_t defaults[4];\n  uint32_t originalColors[4];')
    a=header.index('  static constexpr uint16_t MAX_MURMURATION_BIRDS');header=header[:a]+'};\n'
    h.write_text(header)
    # Gallery IDs are dense, while native engine IDs stay stable for saved patches.
    old_pf=re.search(r'const pfMeta=(\[.*?\]);',ino,re.S)[1]
    old_pf=json.loads(old_pf.replace("'",'"'))
    pf=[[old_pf[i][0],old_pf[i][1]] for i in KEEP_VARIANTS]+[[n,l] for _,n,l,_,_ in NEW]
    ino=re.sub(r'const pfMeta=\[.*?\];',lambda _: 'const pfMeta='+json.dumps(pf,ensure_ascii=False,separators=(',',':'))+';',ino,flags=re.S)
    author_match=re.search(r'const pfAuthors=.*?;',ino,re.S)
    authors=['Patternflow Community']*20+['@WollyMeister','@engmung','@engmung','@martinnlopez','@engmung','Shadertoy · HG_SDF']+['aap · PsiOp']*3+['Imported Arduino sketch']*13
    new_authors=[]
    for slug,*_ in NEW:
        source=pathlib.Path('/Users/xerox/Downloads/untitled folder 2')/('patternflow_'+slug+'.js')
        author=re.search(r'^// Author:\s*(.*)',source.read_text(),re.M)[1]
        new_authors.append(author+' · Patternflow')
        target=ROOT/'patterns/imports/patternflow'/source.name
        if target.exists(): assert target.read_bytes()==source.read_bytes()
        else: shutil.copyfile(source,target)
    ino=ino[:author_match.start()]+'const pfAuthors='+json.dumps([authors[i] for i in KEEP_VARIANTS]+new_authors,separators=(',',':'))+';'+ino[author_match.end():]
    ino=ino.replace('defs.slice(0,16).map((d,i)=>({key:\'n\'+i,name:d.name,engine:i,variant:null,source:\'Built-in C++\',seed:i}))', 'corePatternIds.map(i=>({key:\'n\'+i,name:defs[i].name,engine:i,variant:null,source:\'Built-in C++\',seed:i}))')
    ino=ino.replace('const galleryItems=','const corePatternIds='+json.dumps(KEEP_CORE)+';\nconst galleryItems=',1)
    ino=ino.replace('bool patternBookmarks[16 + PATTERNFLOW_PATTERN_COUNT]', 'bool patternBookmarks[CORE_PATTERN_COUNT + PATTERNFLOW_PATTERN_COUNT]')
    ino=ino.replace('16+PATTERNFLOW_PATTERN_COUNT','GALLERY_PATTERN_COUNT').replace('16 + PATTERNFLOW_PATTERN_COUNT','GALLERY_PATTERN_COUNT')
    ino=ino.replace('#include "src/patterns/PatternflowPack.h"','#include "src/patterns/PatternflowPack.h"\n#include "src/generated/PatternCatalog.h"')
    sizes=[32,32,32,32,32,32,64,64,24,32,64,64,64,32,32,32,64]+[64]*18
    ino=re.sub(r'const uint8_t patternflowRenderSize\[PATTERNFLOW_PATTERN_COUNT\] = \{.*?\};','const uint8_t patternflowRenderSize[PATTERNFLOW_PATTERN_COUNT] = {'+','.join(map(str,sizes))+'};',ino,flags=re.S)
    ino_path.write_text(ino)
    catalog=[{'name':re.search(r"name:'([^']+)'",line)[1],'engine':i,'variant':None} for i,line in enumerate(re.findall(r'^ \{name:.*',ino,re.M)[:16]) if i in KEEP_CORE]
    catalog += [{'name':m[0],'engine':16,'variant':i} for i,m in enumerate(pf)]
    (ROOT/'assets/pattern-catalog.json').write_text(json.dumps(catalog,indent=2)+'\n')
    (ROOT/'src/generated/PatternCatalog.h').write_text('#pragma once\nconstexpr uint8_t CORE_PATTERN_COUNT=11;\nconstexpr uint8_t GALLERY_PATTERN_COUNT=CORE_PATTERN_COUNT+PATTERNFLOW_PATTERN_COUNT;\nconst uint8_t CORE_PATTERN_IDS[CORE_PATTERN_COUNT]={'+','.join(map(str,KEEP_CORE))+'};\nconst uint8_t LEGACY_VARIANT_IDS[17]={'+','.join(map(str,KEEP_VARIANTS))+'};\n')
    print('Retained 28, removed 30, added 18 unique patterns: 46 total.')

if __name__=='__main__':main()
