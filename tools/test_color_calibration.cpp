#include "../src/platform/ColorCalibration.h"
#include <cassert>
#include <cstdio>
int main(){
 for(unsigned c=0;c<65536;c++){
  assert(channelBalanced565(c,100,100,100)==c);
  assert(channelBalanced565(c,0,0,0)==0);
  assert((channelBalanced565(c,0,100,100)&0xf800)==0);
  assert((channelBalanced565(c,100,0,100)&0x07e0)==0);
  assert((channelBalanced565(c,100,100,0)&0x001f)==0);
  const unsigned half=channelBalanced565(c,50,50,50);
  assert(((half>>11)&31)<=((c>>11)&31)&&((half>>5)&63)<=((c>>5)&63)&&(half&31)<=(c&31));
 }
 puts("PASS: exhaustive RGB565 identity, blackout, channel isolation and attenuation");
}
