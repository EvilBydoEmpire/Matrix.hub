#include "../src/patterns/PatternflowPack.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <initializer_list>
struct Frame{uint16_t px[4096]{};};
void put(void*o,int16_t x,int16_t y,uint16_t c){assert(x>=0&&x<64&&y>=0&&y<64);static_cast<Frame*>(o)->px[y*64+x]=c;}
float difference(const Frame&a,const Frame&b){float d=0;for(int i=0;i<4096;i++)for(int s:{0,5,11}){int m=s==5?63:31;d+=abs(int((a.px[i]>>s)&m)-int((b.px[i]>>s)&m))/float(m);}return d/12288;}
int main(){
 PatternflowPack engine;uint16_t pal[256],rgb[4096];
 auto prepare=[&](int v){for(int i=0;i<256;i++){
  auto&colors=PATTERNFLOW_METADATA[v].originalColors;int a=i/64,b=(a+1)&3,k=i%64;unsigned color=0;
  for(int s:{16,8,0}){int x=(colors[a]>>s)&255,y=(colors[b]>>s)&255;color|=(x+(y-x)*k/64)<<s;}
  pal[i]=((color>>8)&0xf800)|((color>>5)&0x7e0)|((color>>3)&31);}
  for(int i=0;i<4096;i++){int r=((i>>8)&15)*17,g=((i>>4)&15)*17,b=(i&15)*17;rgb[i]=((r&248)<<8)|((g&252)<<3)|(b>>3);}
 };
 auto simulate=[&](int v,const uint8_t*p,int fps,float time){engine.reset(v);Frame f;for(int i=0;i<int(fps*time);i++)engine.render(v,64,64,1.f/fps,p,pal,rgb,put,&f);return f;};
 for(int v:{49,50,51,52,53,54,55,59,60,61,62}){
  prepare(v);auto&meta=PATTERNFLOW_METADATA[v];Frame base=simulate(v,meta.defaults,60,5.f),slow=simulate(v,meta.defaults,30,5.f),fast=simulate(v,meta.defaults,120,5.f);
  assert(difference(base,slow)<.01f&&difference(base,fast)<.01f);
  for(int c=0;c<4;c++){uint8_t p[4];memcpy(p,meta.defaults,4);p[c]=0;
   float d=difference(base,simulate(v,p,60,5.f));
   if(d<.001f){printf("INERT %s: %s %.6f\n",meta.name,meta.controlLabel[c],d);return 1;}}
  Frame later=simulate(v,meta.defaults,60,7.f);
  if(v<56||v>=59)assert(difference(base,later)>.004f);else assert(difference(base,later)==0);
  for(auto&c:pal)c=0xf800;for(auto&c:rgb)c=0xf800;
  assert(difference(base,simulate(v,meta.defaults,60,5.f))>.01f);
  memset(pal,0,sizeof(pal));memset(rgb,0,sizeof(rgb));Frame black=simulate(v,meta.defaults,30,1.f);
  for(auto c:black.px)assert(c==0);
  printf("PASS %s: 4 effective controls, 30/60/120 FPS timing, palette + blackout, %s\n",meta.name,(v<56||v>=59)?"moving":"static photo");
 }
 engine.reset(0);
}
