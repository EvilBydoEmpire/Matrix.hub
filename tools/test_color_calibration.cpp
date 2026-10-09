#include "../src/platform/ColorCalibration.h"
#include <cassert>
#include <cstdio>
int main(){
 for(unsigned c=0;c<65536;c++){
  assert(channelBalanced565(c,100,100,100)==c);
  assert(channelBalanced565(c,0,0,0)==0);
  assert((channelBalanced565(c,0,100,100)&0xf800)==0);
  auto q=simulateThreeBit565(c);
  auto check=[](unsigned level,unsigned actual){assert(abs(int(actual)-int(lroundf(level*15.f/7.f)))<=1);};
  check((c>>13)&7,(q>>12)&15);check((c>>8)&7,(q>>7)&15);check((c>>2)&7,(q>>1)&15);
 }
 for(int gamma=100;gamma<=260;gamma++){
  assert(photoGammaValue(0,gamma)==0);assert(photoGammaValue(255,gamma)==255);
  for(int v=1;v<256;v++){assert(photoGammaValue(v,gamma)>=photoGammaValue(v-1,gamma));assert(photoGammaValue(v,100)==v);}
 }
 puts("PASS: exhaustive RGB565 identity/blackout/channel isolation, 3-bit simulation, gamma endpoints/identity/monotonicity");
}
