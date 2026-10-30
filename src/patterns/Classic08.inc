// Raindrops and Plasma, ports of the privately supplied LED_Matrix_08.ino, whose
// loop paused 25 ms per frame: a fixed 30 Hz simulation without blocking or
// per-frame allocation. The sketch was tuned on 16×16, so both draw on a
// 16-pixel logical canvas framed alike at every panel size.
namespace {
struct ClassicDrop { float x,y,radius; int life; uint8_t hue; };
ClassicDrop classicDrops[4]{};
uint16_t* classicTrail=nullptr; // fractional RGB channels, only while active
int classicId=-1,classicW=0,classicH=0;
float classicCarry=FIRST_CARRY,classicTime=0,classicHue=0;
uint32_t classicSeed=1;
void resetClassic08(){
 free(classicTrail);classicTrail=nullptr;classicId=-1;classicW=classicH=0;
 classicCarry=FIRST_CARRY;classicTime=classicHue=0;classicSeed=1;
 for(auto& drop:classicDrops)drop={};
}
void renderClassic08(int id,int W,int H,float dt,const uint8_t p[4],
 const uint16_t pal[256],PatternflowPixelWriter write,void* context){
 if(!classicTrail||classicId!=id||classicW!=W||classicH!=H){
  resetClassic08();
  classicTrail=(uint16_t*)calloc(W*H*3,sizeof(uint16_t));
  if(!classicTrail)return;
  classicId=id;classicW=W;classicH=H;
 }
 const TrailCap cap=trailCap(pal);
 const float step=1.f/30.f,sx=16.f/W,sy=16.f/H;
 for(int tick=fixedSteps(classicCarry,dt,30);tick>0;tick--){
  classicTime+=step*mapControl(p[0],.05f,1.5f);
  classicHue+=id==1?mapControl(p[2],0.f,2.f):0.f;
  if(classicHue>=256.f)classicHue-=256.f;
  int fade=10+p[id==0?1:3]*90/100;
  trailFade(classicTrail,W*H*3,256-fade);
  auto add=[&](int pixel,uint16_t color,float amount) PIXEL_INLINE {trailAdd(classicTrail+pixel*3,color,amount,cap);};
  if(id==0){
   if(randomUnit(classicSeed)*200.f<1+p[0]*9/100){
    for(auto& d:classicDrops)if(d.life<=0){
     d={floorf(randomUnit(classicSeed)*16.f),floorf(randomUnit(classicSeed)*16.f),0,255,uint8_t(64+randomUnit(classicSeed)*20.f)};break;
    }
   }
   const float thickness=mapControl(p[2],.5f,4.f),speed=mapControl(p[3],.05f,.5f);
   for(auto& d:classicDrops)if(d.life>0){
    d.radius+=speed;d.life=max(0,d.life-2);
    // Rings keep the source's hard edges.
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
    float center=fastSqrt((px-7.5f)*(px-7.5f)+(py-7.5f)*(py-7.5f));
    float a=fastSqrt(px*px+(py-15.f)*(py-15.f));
    float b=fastSqrt((px-15.f)*(px-15.f)+py*py);
    float v=smoothSin(center/scale+t)+smoothSin(a/(scale*.8f)+t/.7f)+smoothSin(b/(scale*1.2f)+t/1.3f);
    // The source's quantised hue field, added onto fading trails.
    int hue=(int(v*100.f)+300)*255/600;
    add(y*W+x,pal[(hue+int(classicHue))&255],120.f/255.f);
   }
  }
 }
 for(int y=0;y<H;y++)for(int x=0;x<W;x++)write(context,x,y,trailColor(classicTrail+(y*W+x)*3,cap));
}
}
