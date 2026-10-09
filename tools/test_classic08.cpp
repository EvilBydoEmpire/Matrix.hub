#include "../src/patterns/PatternflowPack.h"
#include "../src/patterns/LibraryMetadata.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
struct Frame{int size;uint16_t pixels[4096]{};};
void put(void* ptr,int16_t x,int16_t y,uint16_t c){auto&f=*static_cast<Frame*>(ptr);assert(x>=0&&y>=0&&x<f.size&&y<f.size);f.pixels[y*f.size+x]=c;}
uint64_t render(int v,int size,int fps,int changed=-1){
 PatternflowPack engine;Frame f{size};uint16_t pal[256],rgb[4096]{};
 for(int i=0;i<256;i++)pal[i]=uint16_t(((i/8)<<11)|((63-i/4)<<5)|((i/8)^31));
 uint8_t p[4];memcpy(p,PATTERNFLOW_METADATA[v].defaults,4);if(changed>=0)p[changed]=100;
 uint64_t hash=1469598103934665603ULL;bool lit=false;
 engine.reset(v);for(int frame=0;frame<fps*10;frame++){
  engine.render(v,size,size,1.f/fps,p,pal,rgb,put,&f);
  if(frame%fps==fps-1)for(int i=0;i<size*size;i++){hash=(hash^f.pixels[i])*1099511628211ULL;lit|=f.pixels[i]!=0;}
 }
 assert(lit); // Sparse rings may legitimately leave the panel between drops.
 memset(pal,0,sizeof(pal));engine.render(v,size,size,1.f/60,p,pal,rgb,put,&f);
 for(int i=0;i<size*size;i++)assert(f.pixels[i]==0); // brightness zero clears trails immediately
 engine.reset(0);return hash;
}
int main(){
 for(int v:{63,64})for(int size:{16,22,64}){
  auto base=render(v,size,30);assert(base==render(v,size,60));assert(base==render(v,size,120));
  for(int c=0;c<4;c++)assert(render(v,size,60,c)!=base);
 }
 using namespace LibraryMeta;Store s;uint8_t old[2232]{};old[0]=1;
 old[4+79*sizeof(Entry)]=5;assert(decodeLegacy(old,sizeof(old),s)&&valid(s));assert(s.entries[79].rating==5&&s.entries[80].rating==0);
 s.entries[80]={4,0,0,1,0};uint8_t bytes[FlashBudget];Store decoded;
 auto n=encode(s,bytes,sizeof(bytes));assert(n&&decode(bytes,n,decoded)&&decoded.entries[80].rating==4&&archived(decoded.entries[80]));
 puts("PASS: Classic 08 sizes, timing, four controls, blackout, legacy/new metadata");
}
