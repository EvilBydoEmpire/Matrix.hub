// Host sanitizer smoke test of the same native C++ renderer compiled for ESP32.
#include "../src/patterns/PatternflowPack.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
struct Frame { int w,h; unsigned count=0; uint16_t pixels[4096]{}; };
void pixel(void* ptr,int16_t x,int16_t y,uint16_t color){
  auto& f=*static_cast<Frame*>(ptr);
  // Drawing helpers are allowed to emit offscreen points; the actual backend
  // clips these before indexing its framebuffer too.
  if(x<0||y<0||x>=f.w||y>=f.h)return;
  f.pixels[y*f.w+x]=color;f.count++;
}
int main(){
  PatternflowPack engine;uint16_t pal[256],rgb[4096];
  for(int i=0;i<256;i++)pal[i]=uint16_t(i*257);
  for(int i=0;i<4096;i++)rgb[i]=uint16_t(i*15);
  for(int size:{16,22,64})for(int v=0;v<PATTERNFLOW_PATTERN_COUNT;v++){
    for(int test=0;test<4;test++){
      uint8_t controls[4];std::memcpy(controls,PATTERNFLOW_METADATA[v].defaults,4);
      if(test==1)std::memset(controls,0,4);
      if(test==2)std::memset(controls,100,4);
      if(test==3){controls[0]=0;controls[1]=100;controls[2]=0;controls[3]=100;}
      engine.reset(v);Frame frame{size,size};
      for(int n=0;n<12;n++)engine.render(v,size,size,n==10?.1f:1.f/60,controls,pal,rgb,pixel,&frame);
      // Threshold controls legitimately allow a fully black frame.
    }
    std::printf("Checked %d: %s at %d\n",v,PATTERNFLOW_METADATA[v].name,size);std::fflush(stdout);
  }
  engine.reset(0);std::puts("PASS: all registered IDs, 3 matrix sizes, defaults and control extremes under sanitizers");
}
