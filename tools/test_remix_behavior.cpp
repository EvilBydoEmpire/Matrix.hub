// Every remix control must affect its image; playback speed must be FPS-independent.
#include "../src/patterns/PatternflowPack.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <initializer_list>
struct Frame {uint16_t pixels[4096]{};};
void put(void*ptr,int16_t x,int16_t y,uint16_t color){
 assert(x>=0&&y>=0&&x<64&&y<64);static_cast<Frame*>(ptr)->pixels[y*64+x]=color;
}
float difference(const Frame&a,const Frame&b){
 double total=0;
 for(int i=0;i<4096;i++)for(int shift:{0,5,11}){
  int mask=shift==5?63:31;total+=std::abs(int((a.pixels[i]>>shift)&mask)-int((b.pixels[i]>>shift)&mask))/float(mask);
 }
 return total/(4096*3);
}
int main(){
 PatternflowPack engine;uint16_t pal[256],rgb[4096]{};
 for(int i=0;i<256;i++)pal[i]=uint16_t((31-(i/8))<<11)|uint16_t((i/4)<<5)|uint16_t(12+i/14);
 auto render=[&](int v,const uint8_t*p,int frames){Frame result;engine.reset(v);
  for(int i=0;i<frames;i++)engine.render(v,64,64,2.f/frames,p,pal,rgb,put,&result);return result;};
 for(int v:{35,36,38,42}){ // Surviving remixes; retired IDs stay tombstones.
  const auto&meta=PATTERNFLOW_METADATA[v];Frame base=render(v,meta.defaults,120);
  Frame slow=render(v,meta.defaults,60);assert(difference(base,slow)<.015f);
  for(int c=0;c<4;c++){
   uint8_t changed[4];std::memcpy(changed,meta.defaults,4);changed[c]=meta.defaults[c]>=50?0:100;
   Frame altered=render(v,changed,120);float amount=difference(base,altered);
   if(amount<.001f){std::printf("No meaningful control effect: %s / %s / %.5f\n",meta.name,meta.controlLabel[c],amount);return 1;}
  }
  std::printf("PASS %s: 4 independent controls + frame-rate-independent motion\n",meta.name);
 }
 engine.reset(0);
}
