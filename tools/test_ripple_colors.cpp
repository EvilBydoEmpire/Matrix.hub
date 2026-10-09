#include "../src/patterns/PatternflowPack.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
struct Frame{int size;unsigned lit=0;};
void writePixel(void* ctx,int16_t x,int16_t y,uint16_t c){
 auto& frame=*static_cast<Frame*>(ctx);
 if(x<0||y<0||x>=frame.size||y>=frame.size)return;
 int r=c>>11,g=(c>>5)&63,b=c&31;
 // A teal ring must stay teal even when another unused palette stop is white.
 // Allow independent RGB565 rounding, but not accumulated channel clipping.
 assert(r<=g*4/42+2);assert(b<=g*20/42+2);
 if(c)frame.lit++;
}
int main(){
 PatternflowPack engine;uint16_t pal[256],rgb[4096]{};
 for(int i=0;i<256;i++)pal[i]=i<16?uint16_t((4<<11)|(42<<5)|20):0xffff;
 uint8_t controls[4]={80,65,100,0};
 for(int size:{16,22,64}){Frame frame{size};engine.reset(48);
  for(int f=0;f<240;f++)engine.render(48,size,size,1.f/60,controls,pal,rgb,writePixel,&frame);
  assert(frame.lit>1000);
 }
 engine.reset(0);puts("PASS additive ripple pigment survives overlap/saturation at all three sizes");
}
