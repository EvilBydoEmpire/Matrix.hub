#pragma once
#include <stdint.h>
#include <math.h>
// Hand-tuned LED/sRGB approximations of the user's photographed print swatches.
// Photo order, left page before right page. Not official measured print values.
namespace Soukan {
constexpr int First=11,Count=10,Total=First+Count;
constexpr uint32_t Colors[Count][4]={
 {0x91CDD1,0x090B0D,0xD5B43F,0xBB672C}, // 1: 255 — Calamine / black / pyrite / sienna
 {0xED622E,0xE09B80,0x176C50,0x090B0D}, // 2: 256 — orange / cinnamon / viridian / black
 {0x596B78,0xC9B296,0xE0B329,0xA04732}, // 3: 249 — blue / ecru / ochre / russet
 {0xBAD7D3,0x289579,0xD8B63D,0xE9542A}, // 4: 250 — Nile / sea green / pyrite / peach red
 {0x008A94,0x2C3C82,0xF1C818,0xC77F4B}, // 5: 247 — benzol / Lyons / apricot / sienna
 {0xB7ABD1,0xDF6B87,0x582D51,0xB68732}, // 6: 248 — lavender / eosine / purple / khaki
 {0x2E3441,0x727042,0xEDD09B,0xC87836}, // 7: 243 — slate / olive / ivory / sienna
 {0x00688C,0x67726B,0xB89991,0xBA542B}, // 8: 244 — Antwerp / green / drab / tawny
 {0x596776,0xF4DF94,0xE83E24,0xC69B50}, // 9: 241 — blue / lemon / red orange / Isabella
 {0x47864D,0xF18C9E,0xB8522B,0x090B0D}  // 10: 242 — diamine / eosine / sienna / black
};
inline bool active(int palette){return palette>=First&&palette<Total;}
// Stable engine/variant IDs, including archived entries. No preset positions change.
constexpr uint8_t Native[16]={9,5,3,3,1,7,6,9,6,4,3,2,6,8,4,4};
constexpr uint8_t Variants[68]={
 4,6,4,5,7,6,9,5,4,6,5,8,1,4,7,3,6,
 7,5,9,8,8,4,4,10,1,7,9,8,5,7,4,9,5,6,
 9,6,5,6,4,4,9,1,6,1,9,5,6,5,
 6,8,9,7,4,1,6,9,7,5,6,5,4,10,5,6,3,4,7
};
inline int paletteFor(int engine,int variant){return First-1+(engine==16?Variants[variant]:Native[engine]);}
inline float colorScale(int engine,int variant){
 if(engine!=16)return engine==1?2.f:(engine==5||engine==6)?3.2f:1.f;
 switch(variant){case 1:case 33:case 39:case 46:return 1.7f;
 case 47:return 3.f;case 60:case 61:case 62:return 2.2f;
 case 9:case 36:return 1.5f;case 48:return 6.f;default:return 1.f;}
}
inline float colorOffset(int engine,int variant){return engine==16&&variant==10?.5f:0.f;}
inline float luminance(uint32_t c){return (((c>>16)&255)*.2126f+((c>>8)&255)*.7152f+(c&255)*.0722f)/255.f;}
inline float ease(float x){x=fmaxf(0.f,fminf(1.f,x));return x*x*(3.f-2.f*x);}
inline uint32_t blend(uint32_t a,uint32_t b,float mix){
 uint32_t out=0;for(int shift=0;shift<=16;shift+=8){float u=(a>>shift)&255,v=(b>>shift)&255;out|=uint32_t(u+(v-u)*mix+.5f)<<shift;}return out;
}
inline uint32_t sample(const uint32_t stops[4],float position){
 position-=floorf(position);float scaled=position*4.f;int i=int(scaled);
 // Broad pigment plateaus with short smooth transitions, not muddy full ramps.
 return blend(stops[i],stops[(i+1)&3],ease((scaled-i-.22f)/.56f));
}
inline void orderedByLight(const uint32_t stops[4],uint32_t result[4]){
 for(int i=0;i<4;i++)result[i]=stops[i];
 for(int i=1;i<4;i++)for(int j=i;j>0&&luminance(result[j])<luminance(result[j-1]);j--){uint32_t c=result[j];result[j]=result[j-1];result[j-1]=c;}
}
inline uint32_t photoTone(const uint32_t ordered[4],float light){
 float s=fmaxf(0.f,fminf(1.f,light))*3.f;int i=int(s);if(i>=3)return ordered[3];
 uint32_t color=blend(ordered[i],ordered[i+1],ease(s-i));
 return light<.09f?blend(0,color,light/.09f):color; // real shadows remain black
}
}
