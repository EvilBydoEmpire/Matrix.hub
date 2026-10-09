// Ports of the user's LED_Matrix_08.ino. Source loop: delay(25) + WS2812 show.
// Use a fixed 30 Hz simulation, a 16-pixel logical canvas, and shared palettes.
// No FastLED dependency, blocking delay, or per-frame allocation.
namespace {
struct ClassicDrop { float x,y,radius; int life; uint8_t hue; };
ClassicDrop classicDrops[4]{};
uint16_t* classicTrail=nullptr; // fractional RGB channels, only while active
int classicId=-1,classicW=0,classicH=0;
float classicCarry=0,classicTime=0,classicHue=0;
uint32_t classicSeed=1,classicPaletteHash=0;
void resetClassic08(){
 free(classicTrail);classicTrail=nullptr;classicId=-1;classicW=classicH=0;
 classicCarry=classicTime=classicHue=0;classicSeed=1;classicPaletteHash=0;
 for(auto& drop:classicDrops)drop={};
}
void renderClassic08(int id,int W,int H,float dt,const uint8_t p[4],
 const uint16_t pal[256],PatternflowPixelWriter write,void* context){
 if(classicId!=id||classicW!=W||classicH!=H){
  resetClassic08();classicId=id;classicW=W;classicH=H;
  classicTrail=(uint16_t*)calloc(W*H*3,sizeof(uint16_t));
 }
 if(!classicTrail)return;
 uint32_t hash=2166136261U;int caps[3]={};
 for(int i=0;i<256;i++){
  hash=(hash^pal[i])*16777619U;
  caps[0]=max(caps[0],int(pal[i]>>11)*2114);
  caps[1]=max(caps[1],int((pal[i]>>5)&63)*1040);
  caps[2]=max(caps[2],int(pal[i]&31)*2114);
 }
 if(hash!=classicPaletteHash){memset(classicTrail,0,W*H*3*sizeof(uint16_t));classicPaletteHash=hash;}
 const float step=1.f/30.f,sx=16.f/W,sy=16.f/H;
 classicCarry+=constrain(dt,0.f,.1f);
 int ticks=min(3,int((classicCarry+1e-6f)/step));classicCarry=max(0.f,classicCarry-ticks*step);
 for(int tick=0;tick<ticks;tick++){
  classicTime+=step*mapControl(p[0],.05f,1.5f);
  classicHue+=id==1?mapControl(p[2],0.f,2.f):0.f;
  if(classicHue>=256.f)classicHue-=256.f;
  int fade=10+p[id==0?1:3]*90/100;
  for(int i=0;i<W*H*3;i++)classicTrail[i]=(uint32_t(classicTrail[i])*(256-fade))>>8;
  auto add=[&](int pixel,uint16_t color,float amount){
   int sum[3]={int(classicTrail[pixel*3])+int((color>>11)*2114*amount),
    int(classicTrail[pixel*3+1])+int(((color>>5)&63)*1040*amount),
    int(classicTrail[pixel*3+2])+int((color&31)*2114*amount)};
   float gain=1.f;
   for(int c=0;c<3;c++)if(sum[c]>caps[c])gain=min(gain,caps[c]/float(sum[c]));
   for(int c=0;c<3;c++)classicTrail[pixel*3+c]=uint16_t(sum[c]*gain);
  };
  if(id==0){
   if(randomUnit(classicSeed)*200.f<1+p[0]*9/100){
    for(auto& d:classicDrops)if(d.life<=0){
     d={floorf(randomUnit(classicSeed)*16.f),floorf(randomUnit(classicSeed)*16.f),0,255,uint8_t(randomUnit(classicSeed)*20.f)};break;
    }
   }
   const float thickness=mapControl(p[2],.5f,4.f),speed=mapControl(p[3],.05f,.5f);
   for(auto& d:classicDrops)if(d.life>0){
    d.radius+=speed;d.life=max(0,d.life-2);
    // Preserve the source's hard ring boundaries rather than soft-glow Ripples.
    float inner=max(0.f,d.radius-thickness*.5f),outer=d.radius+thickness*.5f;
    for(int y=0;y<H;y++)for(int x=0;x<W;x++){
     float dx=(x+.5f)*sx-.5f-d.x,dy=(y+.5f)*sy-.5f-d.y,r2=dx*dx+dy*dy;
     if(r2>=inner*inner&&r2<=outer*outer)add(y*W+x,pal[d.hue],d.life/255.f);
    }
   }
  }else{
   const float scale=mapControl(p[1],2.f,6.f),t=classicTime;
   for(int y=0;y<H;y++)for(int x=0;x<W;x++){
    float px=(x+.5f)*sx-.5f,py=(y+.5f)*sy-.5f;
    float center=fastSqrtApprox((px-7.5f)*(px-7.5f)+(py-7.5f)*(py-7.5f));
    float a=fastSqrtApprox(px*px+(py-15.f)*(py-15.f));
    float b=fastSqrtApprox((px-15.f)*(px-15.f)+py*py);
    float v=silkySin(center/scale+t)+silkySin(a/(scale*.8f)+t/.7f)+silkySin(b/(scale*1.2f)+t/1.3f);
    // Retain the source's quantized hue field and additive persistence.
    int hue=(int(v*100.f)+300)*255/600;
    add(y*W+x,pal[(hue+int(classicHue))&255],120.f/255.f);
   }
  }
 }
 for(int y=0;y<H;y++)for(int x=0;x<W;x++){
  const auto* c=classicTrail+(y*W+x)*3;
  write(context,x,y,uint16_t((min(31,int(c[0]/2114))<<11)|(min(63,int(c[1]/1040))<<5)|min(31,int(c[2]/2114))));
 }
}
}
