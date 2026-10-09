// User's actual photos, not generated substitutes. Static at neutral defaults.
// The palette's Original mode retains source colors; other palettes recolor.
#include "../generated/PersonalPhotos.h"
uint16_t personalPhotoTexel(uint8_t id,uint8_t x,uint8_t y){
 return id<2 && x<64 && y<64 ? PERSONAL_PHOTOS[id][y*64+x] : 0;
}
namespace {
void renderPhotoPattern(uint8_t id,uint16_t W,uint16_t H,const uint8_t p[4],
 const uint16_t rgb[4096],PatternflowPixelWriter writer,void* context){
 // Channel transfer functions are tiny and calculated once per frame, not
 // with pow/HSV conversion for every pixel. Neutral settings are bit-exact.
 uint8_t red[16],green[16],blue[16];
 float exposure=mapControl(p[0],.25f,1.75f),contrast=mapControl(p[1],.35f,1.65f);
 float black=mapControl(p[2],-.25f,.25f),warmth=mapControl(p[3],-.2f,.2f);
 for(int i=0;i<16;i++){
  float value=((i/15.f-.5f)*contrast+.5f+black)*exposure;
  red[i]=(uint8_t)roundf(clamp01(value+warmth)*15.f);
  green[i]=(uint8_t)roundf(clamp01(value)*15.f);
  blue[i]=(uint8_t)roundf(clamp01(value-warmth)*15.f);
 }
 if(W==64&&H==64){
  for(int y=0;y<64;y++)for(int x=0;x<64;x++){
   uint16_t v=PERSONAL_PHOTOS[id][y*64+x];
   writer(context,x,y,rgb[(red[(v>>8)&15]<<8)|(green[(v>>4)&15]<<4)|blue[v&15]]);
  }
  return;
 }
 for(int y=0;y<H;y++)for(int x=0;x<W;x++){
  // Area-average when used on the smaller WROOM panels. Exact texel fetch
  // at 64×64; no resampling or additional allocation on the MatrixPortal.
  int x0=x*64/W,x1=(x+1)*64/W,y0=y*64/H,y1=(y+1)*64/H;
  unsigned r=0,g=0,b=0,n=0;
  for(int sy=y0;sy<y1;sy++)for(int sx=x0;sx<x1;sx++){
   uint16_t v=PERSONAL_PHOTOS[id][sy*64+sx];r+=(v>>8)&15;g+=(v>>4)&15;b+=v&15;n++;
  }
  uint16_t index=(red[(r+n/2)/n]<<8)|(green[(g+n/2)/n]<<4)|blue[(b+n/2)/n];
  writer(context,x,y,rgb[index]);
 }
}
}
