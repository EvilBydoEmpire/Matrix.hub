#include "../src/patterns/SoukanPalettes.h"
#include <cassert>
#include <cstdio>
int main(){
 for(int i=0;i<16;i++)assert(Soukan::active(Soukan::paletteFor(i,0)));
 for(int i=0;i<63;i++)assert(Soukan::active(Soukan::paletteFor(16,i)));
 for(auto& colors:Soukan::Colors){
  for(int i=0;i<4;i++)assert(Soukan::sample(colors,i*.25f)==colors[i]);
  for(int i=-1000;i<1000;i++)assert(Soukan::sample(colors,i*.019f)<=0xffffff);
  uint32_t ordered[4];Soukan::orderedByLight(colors,ordered);
  assert(Soukan::photoTone(ordered,0)==0);
  assert(Soukan::photoTone(ordered,1)==ordered[3]);
  float previous=0;
  // Independent channel rounding can vary by at most one 8-bit luminance LSB.
  for(int i=0;i<=255;i++){float y=Soukan::luminance(Soukan::photoTone(ordered,i/255.f));assert(y+1.f/255>=previous);previous=y;}
 }
 assert(!Soukan::active(10)&&!Soukan::active(21));
 puts("PASS 79 stable-ID assignments, pigment stops, wrapped gradients, monotonic photo luminance and black shadows");
}
