// Temporal sampling studies. Default rate <1 Hz, shallow modulation; not a
// guarantee of photosensitivity safety. Excluded from automatic playback.
// No allocation, RGB noise, global white flashes, or temporal dithering.
namespace {
float strobeClock=0,strobePhase=0;
int strobeIndex=-1;
void resetAmbientStrobes(){strobeClock=strobePhase=0;strobeIndex=-1;}
inline uint16_t strobeShade(uint16_t c,float light){
 const uint32_t k=uint32_t(clamp01(light)*256.f);
 return ((((c>>11)*k)>>8)<<11)|(((((c>>5)&63)*k)>>8)<<5)|(((c&31)*k)>>8);
}
inline float strobePulse(float phase,float duty){
 float d=fabsf(fract(phase)-.5f)*2.f/duty;
 float a=clamp01(1.f-d);return a*a*(3.f-2.f*a);
}
void renderAmbientStrobe(int index,int W,int H,float dt,const uint8_t p[4],
 const uint16_t* pal,PatternflowPixelWriter write,void* context){
 if(strobeIndex!=index){resetAmbientStrobes();strobeIndex=index;}
 dt=constrain(dt,0.f,.1f);strobeClock+=dt;
 const float t=strobeClock,rate=mapControl(p[0],.25f,8.f);
 // Integrate frequency: t*rate would cause wild phase jumps under rate LFOs.
 strobePhase=fract(strobePhase+dt*rate);
 const float depth=unit(p[3]),duty=mapControl(p[2],.08f,.8f);
 const float inv=2.f/min(W,H),cx=(W-1)*.5f,cy=(H-1)*.5f;
 const float beat=strobePhase,gate=strobePulse(beat,duty);
 // A sampled pose is revealed by the pulse, drifting pose remains underneath.
 const float held=t+(.5f-beat)/rate;
 const float density=mapControl(p[1],3.f,11.f);
 for(int y=0;y<H;y++)for(int x=0;x<W;x++){
  const float px=(x-cx)*inv,py=(y-cy)*inv;
  float shape,light,color;
  if(index==0){
   // Chronophotographic iris: phase-locked petals appear to freeze/reverse.
   const float r=polarRadius[y*W+x]*2.f,a=polarAngle[y*W+x];
   const float pose=t*(1.f-depth*gate)+held*depth*gate;
   const float petals=.5f+.5f*silkyCos(a*floorf(density)+r*5.f-pose*1.7f);
   const float band=.5f+.5f*silkySin(r*11.f-pose*.8f);
   shape=petals*petals*(.3f+.7f*band)*clamp01((1.55f-r)*1.5f);
   light=shape*(.48f*(1.f-depth)+.8f*depth*gate);
   color=.1f+.42f*r+.07f*band;
  }else if(index==1){
   // Rolling shutter: narrow traveling exposures across a liquid curtain.
   const float warp=py+.15f*silkySin(px*3.f+t*.35f);
   const float slits=.5f+.5f*silkyCos(px*density*2.f+.8f*silkySin(py*3.f-t*.3f));
   const float scan=strobePulse(beat-warp*.42f,duty);
   shape=slits*slits*(.55f+.45f*silkySin(py*1.4f+1.5f));
   light=shape*(.42f*(1.f-depth)+.78f*depth*scan);
   color=.15f+.55f*(py+1.f)*.5f;
  }else{
   // Phase garden: each lantern has its own phase, never a synchronized flash.
   const int cells=3+int(unit(p[1])*5.f);
   float ux=(px+1.f)*.5f*cells,uy=(py+1.f)*.5f*cells;
   int ix=int(floorf(ux)),iy=int(floorf(uy));
   float dx=fract(ux)-.5f,dy=fract(uy)-.5f;
   float phase=fract(ix*.381966f+iy*.618034f);
   float pulse=strobePulse(beat+phase,duty);
   float radius=.24f+.055f*silkySin(t*.4f+phase*6.28f);
   float radial=sqrtf(dx*dx+dy*dy);
   shape=clamp01((radius-radial)*cells*1.8f);
   light=shape*(.5f*(1.f-depth)+.8f*depth*pulse);
   color=.08f+phase*.65f;
  }
  write(context,x,y,strobeShade(pal[uint8_t(clamp01(color)*255.f)],light));
 }
}
}
