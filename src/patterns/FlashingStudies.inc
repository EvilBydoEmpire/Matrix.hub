// Flashing studies. Defaults pulse below 1 Hz at shallow depth, which does not
// make them safe for photosensitive viewers; cycling and boot skip them.
namespace {
float flashClock=0,flashPhase=0;
int flashIndex=-1;
void resetFlashingStudies(){flashClock=flashPhase=0;flashIndex=-1;}
inline float flashPulse(float phase,float duty){
 float d=fabsf(fract(phase)-.5f)*2.f/duty;
 float a=clamp01(1.f-d);return a*a*(3.f-2.f*a);
}
void renderFlashingStudy(int index,int W,int H,float dt,const uint8_t p[4],
 const uint16_t* pal,PatternflowPixelWriter write,void* context){
 if(flashIndex!=index){resetFlashingStudies();flashIndex=index;}
 flashClock+=dt;
 const float t=flashClock,rate=mapControl(p[0],.25f,8.f);
 // Integrate frequency: t*rate would jump in phase whenever the rate changes.
 flashPhase=fract(flashPhase+dt*rate);
 const float depth=unit(p[3]),duty=mapControl(p[2],.08f,.8f);
 const float inv=2.f/min(W,H),cx=(W-1)*.5f,cy=(H-1)*.5f;
 const float beat=flashPhase,gate=flashPulse(beat,duty);
 // A sampled pose is revealed by the pulse, drifting pose remains underneath.
 const float held=t+(.5f-beat)/rate;
 const float density=mapControl(p[1],3.f,11.f);
 // Lantern Garden: each lantern's phase, pulse and radius, once per frame.
 const int cells=3+int(unit(p[1])*5.f);
 float lanternPhase[8][8],lanternPulse[8][8],lanternRadius[8][8];
 if(index==2)for(int iy=0;iy<cells;iy++)for(int ix=0;ix<cells;ix++){
  const float phase=fract(ix*.381966f+iy*.618034f);
  lanternPhase[iy][ix]=phase;lanternPulse[iy][ix]=flashPulse(beat+phase,duty);
  lanternRadius[iy][ix]=.24f+.055f*smoothSin(t*.4f+phase*6.28f);
 }
 for(int y=0;y<H;y++)for(int x=0;x<W;x++){
  const float px=(x-cx)*inv,py=(y-cy)*inv;
  float shape,light,color;
  if(index==0){
   // Chronophotographic iris: phase-locked petals appear to freeze/reverse.
   const float r=polarRadius[y*W+x]*2.f,a=polarAngle[y*W+x];
   const float pose=t*(1.f-depth*gate)+held*depth*gate;
   const float petals=.5f+.5f*smoothCos(a*floorf(density)+r*5.f-pose*1.7f);
   const float band=.5f+.5f*smoothSin(r*11.f-pose*.8f);
   shape=petals*petals*(.3f+.7f*band)*clamp01((1.55f-r)*1.5f);
   light=shape*(.48f*(1.f-depth)+.8f*depth*gate);
   color=.1f+.42f*r+.07f*band;
  }else if(index==1){
   // Rolling shutter: narrow travelling exposures across a liquid curtain.
   const float warp=py+.15f*smoothSin(px*3.f+t*.35f);
   const float slits=.5f+.5f*smoothCos(px*density*2.f+.8f*smoothSin(py*3.f-t*.3f));
   const float scan=flashPulse(beat-warp*.42f,duty);
   shape=slits*slits*(.55f+.45f*smoothSin(py*1.4f+1.5f));
   light=shape*(.42f*(1.f-depth)+.78f*depth*scan);
   color=.15f+.55f*(py+1.f)*.5f;
  }else{
   // Lantern Garden: each lantern has its own phase, never a synchronised flash.
   float ux=(px+1.f)*.5f*cells,uy=(py+1.f)*.5f*cells;
   int ix=floorInt(ux),iy=floorInt(uy);
   float dx=ux-ix-.5f,dy=uy-iy-.5f;
   // A panel wider than tall repeats the edge lanterns.
   ix=constrain(ix,0,cells-1);iy=constrain(iy,0,cells-1);
   const float phase=lanternPhase[iy][ix],radius=lanternRadius[iy][ix],radial2=dx*dx+dy*dy;
   // Only pixels inside a lantern need its distance.
   shape=radial2<radius*radius?clamp01((radius-sqrtf(radial2))*cells*1.8f):0.f;
   light=shape*(.5f*(1.f-depth)+.8f*depth*lanternPulse[iy][ix]);
   color=.08f+phase*.65f;
  }
  write(context,x,y,scale565(pal[uint8_t(clamp01(color)*255.f)],light));
 }
}
}
