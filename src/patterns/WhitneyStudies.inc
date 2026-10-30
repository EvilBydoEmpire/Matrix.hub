// Ports of two privately supplied sketches: Kenny Kim's recreation of John
// Whitney's Experiments in Motion Graphics (1968), and Chromatic Bloom, whose
// author is unknown. Neither carries the Patternflow CC-BY-SA terms. Both draw
// on the remix canvas with source-over blending: Motion Graphics as antialiased
// lines, Chromatic Bloom one pixel per particle.
namespace {
// Chromatic Bloom's counterpoint integrates its control, so moving it never
// jumps the ribbons.
float whitneyClock=0,whitneyCounterpoint=0;
void resetWhitneyPattern(){whitneyClock=whitneyCounterpoint=0;}
void renderWhitneyPattern(uint8_t id,uint16_t W,uint16_t H,float dt,const uint8_t p[4],
 const uint16_t pal[256],const SourceColors& sourceColors,PatternflowPixelWriter writer,void*context){
 if(!claimCanvas(id+100,W,H,resetWhitneyPattern))return;
 const float seconds=dt*mapControl(p[0],.1f,1.9f);
 const float cx=(W-1)*.5f,cy=(H-1)*.5f,S=min(W,H)/64.f;
 whitneyClock+=seconds;
 uint16_t background=id==0?sourceColor(sourceColors,0x07181D):id==2?sourceColor(sourceColors,0x171815):0;
 for(int pixel=0;pixel<W*H;pixel++)remixPixels[pixel]=background;
 switch(id){
 case 0:{ // Two 31-line harmonic ribbons, cream first then magenta.
  const int lines=7+(p[1]*48+50)/100;
  const float phaseStep=mapControl(p[2],.2f,1.8f);
  // The fourth control stays at 50, the source's line width.
  const float t=whitneyClock*72.f,width=max(.45f,mapControl(p[3],.45f,1.19f)*S);
  const float scale=min(W/1080.f,H/810.f);
  const uint16_t cream=sourceColor(sourceColors,0xDCD5BF),pink=sourceColor(sourceColors,0xA34A80);
  for(int layer=0;layer<2;layer++)for(int i=0;i<lines;i++){
   float phase=t+i*phaseStep;
   float x1=(layer?fastCos(phase/5.f):fastSin(phase/5.f))*100.f;
   float x2=(layer?fastCos(phase/20.f):fastSin(phase/20.f))*200.f;
   RemixPoint a={cx+x1*scale,cy+fastSin(phase/10.f)*150.f*scale};
   RemixPoint b={cx+x2*scale,cy+fastSin(phase/40.f)*350.f*scale};
   remixLine<remixOver>(a,b,layer?pink:cream,1.f,width);
  }
  break;
 }
 case 2:{ // Chromatic counterpoint: seven pixel ribbons unfold over a square.
  static const uint32_t colors[7]={0xFBFAF8,0xF3EA83,0xC85D4D,0x92302F,0x568C44,0x4A64C7,0xCF6DA0};
  uint16_t ink[7];for(int i=0;i<7;i++)ink[i]=sourceColor(sourceColors,colors[i]);
  const int count=scaledCount(p[1],14,90,10,min(W,H)/64.f);
  const float t=whitneyClock*.42f,fold=mapControl(p[2],.4f,3.4f);
  whitneyCounterpoint+=seconds*.42f*unit(p[3]);
  const float canon=whitneyCounterpoint;
  for(int ribbon=0;ribbon<7;ribbon++)for(int i=0;i<count;i++){
   float u=i/float(count-1),phase=ribbon*(2.f*PI/7.f);
   // Ribbons drift apart at their own rates: coherent petals become
   // interleaved pinwheels and then a bloom woven across the square.
   float a=u*2.f*PI+phase+t*.25f+canon*ribbon*.035f;
   float wave=smoothSin(u*2.f*PI*2.f+phase+t*.7f+canon*ribbon*.11f);
   float r=.12f+.88f*(.5f+.5f*wave);
   a+=fold*smoothSin(u*2.f*PI-t*.47f+phase*.5f)*(.25f+.75f*r);
   float dx=smoothCos(a),dy=smoothSin(a);
   float scale=r/max(.0001f,max(fabsf(dx),fabsf(dy)));
   int x=(int)roundf(cx+dx*scale*cx),y=(int)roundf(cy+dy*scale*cy);
   remixOver(x,y,ink[ribbon],.7f+.3f*u);
  }
  break;
 }
 }
 flushCanvas(writer,context);
}
} // namespace
