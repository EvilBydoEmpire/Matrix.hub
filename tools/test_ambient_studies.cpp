#include "../src/patterns/PatternflowPack.h"
#include "../src/patterns/ArtDirection.h"
#include "../src/generated/PatternCatalog.h"
#include "../src/patterns/LibraryMetadata.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <chrono>
struct Frame{int size;uint64_t hash=0;unsigned count=0;};
void pixel(void* p,int16_t x,int16_t y,uint16_t c){auto& f=*(Frame*)p;assert(x>=0&&y>=0&&x<f.size&&y<f.size);f.count++;f.hash=(f.hash^c)*1099511628211ULL;}
int main(){
 PatternflowPack pack;uint16_t pal[256],rgb[4096]{};for(int i=0;i<256;i++)pal[i]=0xffff;
 for(int size:{16,22,64})for(int v=65;v<68;v++){
  auto run=[&](const uint8_t* p){pack.reset(v);Frame f{size};for(int n=0;n<180;n++)pack.render(v,size,size,1.f/60,p,pal,rgb,pixel,&f);assert(f.count==unsigned(180*size*size));return f.hash;};
  uint8_t p[4];memcpy(p,PATTERNFLOW_METADATA[v].defaults,4);auto a=run(p);assert(a==run(p));
  for(int c=0;c<4;c++){p[c]=0;auto lo=run(p);p[c]=100;auto hi=run(p);assert(lo!=hi);p[c]=PATTERNFLOW_METADATA[v].defaults[c];}
 }
 for(int start=0;start<GALLERY_PATTERN_COUNT;start++){
  int next=LibraryMeta::nextActive(start,GALLERY_PATTERN_COUNT,[](int i){return i>=CORE_PATTERN_COUNT&&ArtDirection::isStrobe(COLLECTION_PATTERN_IDS[i-CORE_PATTERN_COUNT]);});
  assert(next>=0);assert(next<CORE_PATTERN_COUNT||!ArtDirection::isStrobe(COLLECTION_PATTERN_IDS[next-CORE_PATTERN_COUNT]));
 }
 puts("PASS: full-resolution writes, deterministic reset, all four controls effective at 16/22/64, no strobe auto-cycle");
}
