// Whitney-specific timing, source-color, palette-routing and control tests.
#include "../src/patterns/PatternflowPack.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
struct Frame{uint16_t pixels[4096]{};};
void put(void*ptr,int16_t x,int16_t y,uint16_t c){assert(x>=0&&x<64&&y>=0&&y<64);static_cast<Frame*>(ptr)->pixels[y*64+x]=c;}
float difference(const Frame&a,const Frame&b){
 float total=0;for(int i=0;i<4096;i++)for(int shift:{0,5,11}){int mask=shift==5?63:31;
  total+=std::abs(int((a.pixels[i]>>shift)&mask)-int((b.pixels[i]>>shift)&mask))/float(mask);}
 return total/(4096*3);
}
int main(){
 PatternflowPack engine;uint16_t pal[256],rgb[4096];
 auto prepare=[&](int v){
  const auto&meta=PATTERNFLOW_METADATA[v];
  for(int i=0;i<256;i++){int a=i/64,b=(a+1)&3,k=i%64;unsigned c=0;
   for(int shift:{16,8,0}){int u=(meta.originalColors[a]>>shift)&255,w=(meta.originalColors[b]>>shift)&255;c|=(u+(w-u)*k/64)<<shift;}
   pal[i]=((c>>8)&0xF800)|((c>>5)&0x7E0)|((c>>3)&31);}
  for(int i=0;i<4096;i++){int r=((i>>8)&15)*17,g=((i>>4)&15)*17,b=(i&15)*17;rgb[i]=((r&248)<<8)|((g&252)<<3)|(b>>3);}
 };
 auto simulate=[&](int v,const uint8_t*p,int frames,float time){Frame f;engine.reset(v);
  for(int i=0;i<frames;i++)engine.render(v,64,64,time/frames,p,pal,rgb,put,&f);return f;};
 for(int v:{43,45,46,47,48}){
  prepare(v);const auto&meta=PATTERNFLOW_METADATA[v];Frame base=simulate(v,meta.defaults,720,6.f),slow=simulate(v,meta.defaults,180,6.f);
  assert(difference(base,slow)<.01f);
  for(int c=0;c<4;c++){uint8_t p[4];memcpy(p,meta.defaults,4);p[c]=0;
   Frame altered=simulate(v,p,360,6.f);float d=difference(base,altered);
   if(d<.001f){std::printf("Inert control: %s / %s / %.6f\n",meta.name,meta.controlLabel[c],d);return 1;}}
  // Existing RGB palette map must control every colored study; the monochrome
  // column study instead uses the editable gradient independently per column.
  for(auto&c:rgb)c=0xF800;for(auto&c:pal)c=0xF800;
  Frame recolored=simulate(v,meta.defaults,360,6.f);
  assert(difference(base,recolored)>.01f);
  std::printf("PASS %s: four controls, frame-rate-independent motion, palette routing\n",meta.name);
 }
 prepare(43);auto controls=PATTERNFLOW_METADATA[43].defaults;
 Frame holdStart=simulate(43,controls,192,3.2f),holdEnd=simulate(43,controls,216,3.6f),moving=simulate(43,controls,240,4.f);
 assert(difference(holdStart,holdEnd)>.005f);assert(difference(holdEnd,moving)>.005f);
 std::puts("PASS Motion Graphics: no hold during the former 800 ms pause");
 prepare(45);Frame particles=simulate(45,PATTERNFLOW_METADATA[45].defaults,360,6.f);
 int lit=0;for(auto c:particles.pixels)if(c!=rgb[0x111])lit++;
 int count=7*(14+PATTERNFLOW_METADATA[45].defaults[1]*76/100);
 assert(lit>200&&lit<=count);std::puts("PASS Bloom: square-spanning single-pixel ribbons, no discs");
 prepare(48);simulate(48,PATTERNFLOW_METADATA[48].defaults,360,6.f);
 memset(pal,0,sizeof(pal));Frame black;engine.render(48,64,64,1.f/120,PATTERNFLOW_METADATA[48].defaults,pal,rgb,put,&black);
 for(auto c:black.pixels)assert(c==0);
 std::puts("PASS Ripples: existing additive trails respect zero brightness immediately");
 engine.reset(0);
}
