// Offline preview uses the exact firmware renderer; writes packed RGB frames.
#include "../src/patterns/PatternflowPack.h"
#include "../src/patterns/SoukanPalettes.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
struct Frame{uint16_t pixels[4096]{};};
void put(void* ptr,int16_t x,int16_t y,uint16_t c){if(x>=0&&y>=0&&x<64&&y<64)static_cast<Frame*>(ptr)->pixels[y*64+x]=c;}
int main(int argc,char**argv){
 if(argc!=3&&argc!=4){std::fprintf(stderr,"usage: render_remix_preview variant output.rgb [soukan]\n");return 1;}
 int v=atoi(argv[1]);if(v<0||v>=PATTERNFLOW_PATTERN_COUNT)return 2;
 FILE*out=fopen(argv[2],"wb");if(!out)return 3;
 PatternflowPack engine;Frame frame;uint16_t pal[256],rgb[4096]{};
 auto&meta=PATTERNFLOW_METADATA[v];
 for(int i=0;i<256;i++){int a=i/64,b=(a+1)&3,k=i%64;unsigned c=0;
  for(int shift:{16,8,0}){int u=(meta.originalColors[a]>>shift)&255,w=(meta.originalColors[b]>>shift)&255;c|=(u+(w-u)*k/64)<<shift;}
  pal[i]=((c>>8)&0xF800)|((c>>5)&0x07E0)|((c>>3)&31);
 }
 for(int i=0;i<4096;i++){
  unsigned r=((i>>8)&15)*17,g=((i>>4)&15)*17,b=(i&15)*17;
  rgb[i]=((r&248)<<8)|((g&252)<<3)|(b>>3);
 }
 if(argc==4&&!strcmp(argv[3],"soukan"))for(int i=0;i<256;i++){
  auto c=Soukan::sample(Soukan::Colors[Soukan::paletteFor(16,v)-Soukan::First],i/255.f*Soukan::colorScale(16,v)+Soukan::colorOffset(16,v));
  pal[i]=((c>>8)&0xF800)|((c>>5)&0x07E0)|((c>>3)&31);
 }
 engine.reset(v);
 // Eight seconds of motion at 30 FPS; renderer still advances at 60 FPS.
 for(int f=0;f<480;f++){
  std::memset(frame.pixels,0,sizeof(frame.pixels));
  engine.render(v,64,64,1.f/60,meta.defaults,pal,rgb,put,&frame);
  if(f%2)continue;
  for(auto c:frame.pixels){unsigned char px[3]={static_cast<unsigned char>(((c>>11)&31)*255/31),static_cast<unsigned char>(((c>>5)&63)*255/63),static_cast<unsigned char>((c&31)*255/31)};fwrite(px,1,3,out);}
 }
 engine.reset(0);fclose(out);return 0;
}
