// Native ports of the three sketches supplied by the user, October 3, 2026.
// Study 1: Kenny Kim recreation of John Whitney's Experiments in Motion
// Graphics (1968). Authors/licenses of sketches 2/3 were not supplied.
// These snippets are NOT automatically relicensed as Patternflow CC-BY-SA.
// Reuse the transient RGB565 canvas, clipping and antialiased line rasterizer.
namespace {
inline float silkySin(float angle);
inline float silkyCos(float angle);
float whitneyClock=0,whitneyCarry=0;
float whitneyCycles[3]={0,0,0};
float whitneyEnvelopePhase[3]={0,0,0};
void resetWhitneyPattern(){whitneyClock=whitneyCarry=0;for(float&v:whitneyCycles)v=0;for(float&v:whitneyEnvelopePhase)v=0;}
uint16_t whitneySourceColor(uint32_t color,const uint16_t rgb[4096]){
 return rgb[((color>>12)&0xF00)|((color>>8)&0xF0)|((color>>4)&0xF)];
}
// Source-over keeps the seven original square colors distinct at overlaps.
void whitneyOver(int x,int y,uint16_t color,float coverage){
 if(x<0||y<0||x>=remixW||y>=remixH||coverage<=0)return;
 const int a=min(256,(int)(coverage*256.f)),inverse=256-a;
 const uint16_t old=remixPixels[y*remixW+x];
 int r=(((color>>11)*a+(old>>11)*inverse)+128)>>8;
 int g=((((color>>5)&63)*a+((old>>5)&63)*inverse)+128)>>8;
 int b=(((color&31)*a+(old&31)*inverse)+128)>>8;
 remixPixels[y*remixW+x]=(r<<11)|(g<<5)|b;
}
void whitneySquare(float x,float y,float size,uint16_t color){
 float half=size*.5f,left=x-half,right=x+half,top=y-half,bottom=y+half;
 for(int py=max(0,floorInt(top));py<=min(remixH-1,(int)ceilf(bottom));py++){
  float cy=clamp01(min(py+.5f,bottom)-max(py-.5f,top));
  for(int px=max(0,floorInt(left));px<=min(remixW-1,(int)ceilf(right));px++){
   float cx=clamp01(min(px+.5f,right)-max(px-.5f,left));
   whitneyOver(px,py,color,cx*cy);
  }
 }
}
void whitneyDot(float x,float y,float diameter,uint16_t color){
 if(diameter<=1.05f){remixPoint(x,y,color,diameter);return;}
 const float radius=diameter*.5f;
 for(int py=max(0,floorInt(y-radius));py<=min(remixH-1,(int)ceilf(y+radius));py++)
  for(int px=max(0,floorInt(x-radius));px<=min(remixW-1,(int)ceilf(x+radius));px++)
   whitneyOver(px,py,color,clamp01(radius+.5f-fastHypotApprox(px-x,py-y)));
}

// Original per-frame differential speeds, integrated at a fixed virtual 60 Hz.
// The second snippet copied the 72-cycle upper boundary (68) to the 90/120
// cycles. Derive both ends from each period to restore symmetric slow passages.
void stepWhitneyColumns(float seconds,float divergence){
 whitneyCarry+=seconds*60.f;
 const int ticks=min(24,(int)(whitneyCarry+1e-5f));whitneyCarry=max(0.f,whitneyCarry-ticks);
 const float originalRates[3]={5.f,3.f,4.f};
 for(int step=0;step<ticks;step++)for(int band=0;band<3;band++){
  const float rate=4.f+(originalRates[band]-4.f)*divergence;
  float& c=whitneyCycles[band];
  float stepSize=1.f-fastCosApprox(whitneyEnvelopePhase[band]*(PI/180.f));
  float phase=whitneyEnvelopePhase[band]+stepSize*rate;
  phase-=floorf(phase/360.f)*360.f;
  const float edge=min(phase,360.f-phase)/rate;
  if(edge<4.f){float bias=.001f+.099f*edge*.25f;stepSize+=bias;phase+=bias*rate;}
  whitneyEnvelopePhase[band]=phase-floorf(phase/360.f)*360.f;
  c+=stepSize;c-=floorf(c/360.f)*360.f;
  // Both phases stay bounded: the .001 slow-passage increment must remain
  // representable after hours of continuous playback, including custom rates.
 }
}

void renderWhitneyPattern(uint8_t id,uint16_t W,uint16_t H,float dt,const uint8_t p[4],
 const uint16_t pal[256],const uint16_t rgb[4096],PatternflowPixelWriter writer,void*context){
 if(remixIndex!=int(id)+100||remixW!=W||remixH!=H){
  resetRemixPattern();resetWhitneyPattern();remixW=W;remixH=H;remixIndex=id+100;
  remixPixels=(uint16_t*)calloc(W*H,sizeof(uint16_t));
 }
 if(!remixPixels)return;
 const float seconds=constrain(dt,0.f,.1f)*mapControl(p[0],.1f,1.9f);
 const float cx=(W-1)*.5f,cy=(H-1)*.5f,S=min(W,H)/64.f;
 whitneyClock+=seconds;
 uint16_t background=id==0?whitneySourceColor(0x07181D,rgb):id==2?whitneySourceColor(0x171815,rgb):0;
 for(int pixel=0;pixel<W*H;pixel++)remixPixels[pixel]=background;
 switch(id){
 case 0:{ // Two 31-line harmonic ribbons, cream first then magenta.
  const int lines=7+(p[1]*48+50)/100;
  const float phaseStep=mapControl(p[2],.2f,1.8f);
  // Continuous time: even old saved presets with nonzero c3 no longer pause.
  // c3 now controls line width, with 50 retaining the previous default width.
  const float t=whitneyClock*72.f,width=max(.45f,mapControl(p[3],.45f,1.19f)*S);
  const float scale=min(W/1080.f,H/810.f);
  const uint16_t cream=whitneySourceColor(0xDCD5BF,rgb),pink=whitneySourceColor(0xA34A80,rgb);
  for(int layer=0;layer<2;layer++)for(int i=0;i<lines;i++){
   float phase=t+i*phaseStep;
   float x1=(layer?fastCosApprox(phase/5.f):fastSinApprox(phase/5.f))*100.f;
   float x2=(layer?fastCosApprox(phase/20.f):fastSinApprox(phase/20.f))*200.f;
   RemixPoint a={cx+x1*scale,cy+fastSinApprox(phase/10.f)*150.f*scale};
   RemixPoint b={cx+x2*scale,cy+fastSinApprox(phase/40.f)*350.f*scale};
   remixLine<whitneyOver>(a,b,layer?pink:cream,1.f,width);
  }
  break;
 }
 case 2:{ // Chromatic counterpoint: seven pixel ribbons unfold over a square.
  static const uint32_t colors[7]={0xFBFAF8,0xF3EA83,0xC85D4D,0x92302F,0x568C44,0x4A64C7,0xCF6DA0};
  uint16_t ink[7];for(int i=0;i<7;i++)ink[i]=whitneySourceColor(colors[i],rgb);
  const int count=14+p[1]*76/100;
  const float t=whitneyClock*.42f,fold=mapControl(p[2],.4f,3.4f),canon=unit(p[3]);
  for(int ribbon=0;ribbon<7;ribbon++)for(int i=0;i<count;i++){
   float u=i/float(count-1),phase=ribbon*(2.f*PI/7.f);
   // Organized differential motion, not random scatter: coherent petals
   // become interleaved pinwheels and then a square-spanning woven bloom.
   float a=u*2.f*PI+phase+t*(.25f+canon*ribbon*.035f);
   float wave=silkySin(u*2.f*PI*2.f+phase+t*(.7f+canon*ribbon*.11f));
   float r=.12f+.88f*(.5f+.5f*wave);
   a+=fold*silkySin(u*2.f*PI-t*.47f+phase*.5f)*(.25f+.75f*r);
   float dx=silkyCos(a),dy=silkySin(a);
   float scale=r/max(.0001f,max(fabsf(dx),fabsf(dy)));
   int x=(int)roundf(cx+dx*scale*cx),y=(int)roundf(cy+dy*scale*cy);
   // Exactly one pixel per particle; no discs, halos or artificial dithering.
   whitneyOver(x,y,ink[ribbon],.7f+.3f*u);
  }
  break;
 }
 }
 for(int y=0;y<H;y++)for(int x=0;x<W;x++)writer(context,x,y,remixPixels[y*W+x]);
}
} // namespace
