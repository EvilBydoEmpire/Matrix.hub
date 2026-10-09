// Native-resolution developments of the saved Topo Vortex and Checkerboard,
// plus a port of the user's five-ring Ripples sketch. No resolution reduction.
namespace {
struct AdvancedRipple {float x,y,radius,life;uint8_t hue;};
AdvancedRipple advancedRipples[5]{};
uint16_t* rippleTrail=nullptr; // RGB, Q8 channel precision: fade without banding.
float rippleCarry=0;
uint32_t rippleSeed=0x71C9AE13;
void resetAdvancedPattern(){
 free(rippleTrail);rippleTrail=nullptr;rippleCarry=0;rippleSeed=0x71C9AE13;
 for(auto&r:advancedRipples)r={};
}
// Interpolated LUT for subpixel trajectories; unlike a quantized phase lookup,
// this retains small movements at slow speed. No transcendental per point.
inline float silkySin(float a){
 float pos=a*FAST_SIN_SCALE;int n=floorInt(pos);float f=pos-n;
 float first=fastSinTable[uint32_t(n)&FAST_SIN_MASK];
 return (first+(fastSinTable[uint32_t(n+1)&FAST_SIN_MASK]-first)*f)*(1.f/32767.f);
}
inline float silkyCos(float a){return silkySin(a+PI*.5f);}
void spawnAdvancedRipple(uint16_t W,uint16_t H){
 for(auto&r:advancedRipples)if(r.life<=0){
  r.x=randomUnit(rippleSeed)*(W-1);r.y=randomUnit(rippleSeed)*(H-1);
  r.radius=0;r.life=255;r.hue=(uint8_t)(randomUnit(rippleSeed)*10.f);break;
 }
}
void renderAdvancedPattern(uint8_t id,uint16_t W,uint16_t H,float dt,const uint8_t p[4],
 const uint16_t pal[256],PatternflowPixelWriter writer,void* context){
 if(remixIndex!=int(id)+200||remixW!=W||remixH!=H){
  resetRemixPattern();resetAdvancedPattern();remixW=W;remixH=H;remixIndex=id+200;
  remixPixels=(uint16_t*)calloc(W*H,sizeof(uint16_t));
  if(id==2){rippleTrail=(uint16_t*)calloc(W*H*3,sizeof(uint16_t));spawnAdvancedRipple(W,H);}
 }
 if(!remixPixels||(id==2&&!rippleTrail))return;
 const float elapsed=constrain(dt,0.f,.1f),S=min(W,H)/64.f;
 remixClock+=elapsed*mapControl(p[id==0?1:0],.1f,3.3f);
 const float t=remixClock;
 memset(remixPixels,0,W*H*sizeof(uint16_t));
 if(id==0){ // Abyssal: dashed concentric strands braid around two living cores.
  const int rings=6+p[0]*14/100;
  const float twist=unit(p[2])*4.5f,braid=unit(p[3]);
  // Angular harmonics shared by every contour. Each ring has its own traveling
  // phase, radial breathing, toroidal shear and bright moving wave packets.
  constexpr int N=128;float sn[N],cs[N],s2[N],s3[N],s5[N];
  for(int j=0;j<N;j++){float a=j*(2.f*PI/N);sn[j]=silkySin(a);cs[j]=silkyCos(a);
   s2[j]=silkySin(a*2.f-t*.73f);s3[j]=silkySin(a*3.f+t*.41f);s5[j]=silkySin(a*5.f-t*1.1f);}
  for(int core=0;core<2;core++){
   float dir=core?-1.f:1.f;
   float ox=W*(core?.22f:.62f)+silkySin(t*.23f+core*PI)*W*.055f*braid;
   float oy=H*(core?.72f:.28f)+silkyCos(t*.31f+core*PI)*H*.055f*braid;
   for(int ring=rings;ring>0;ring--){
    float k=ring/float(rings),base=(2.f+ring*3.6f)*S;
    float phase=t*.35f*dir+twist*8.f/(base/S+8.f)+silkySin(t*.37f+k*5.f)*braid*.27f;
    float ca=silkyCos(phase),sa=silkySin(phase);
    float breath=silkySin(k*8.f-t*1.6f+core*2.f);
    int stride=ring<4?5:ring<8?3:2;
    uint16_t color=pal[phaseByte(k*190.f)];
    for(int j=0;j<N;j+=stride){
     float deform=s2[j]*(.11f+.045f*breath)+s3[j]*braid*.06f+s5[j]*braid*.024f;
     float r=base*(1.f+deform)+breath*1.8f*S;
     float ux=cs[j]*ca-sn[j]*sa,uy=sn[j]*ca+cs[j]*sa;
     float x=ox+ux*r+s3[j]*braid*2.f*S,y=oy+uy*r*.92f+s2[j]*2.f*S;
     float packet=.5f+.5f*silkySin(j*.147f+k*11.f-t*2.1f*dir);
     float light=.64f+.36f*packet*packet;
     remixPoint(x,y,color,light*1.65f);
     // Short tangent glints, still separated dashes rather than large balls.
     if(packet>.82f&&braid>.05f)remixPoint(x-uy*.7f*S,y+ux*.7f*S,color,braid*.75f);
    }
   }
  }
 }else if(id==1){ // Interference: the original one-pixel lattice, three wave lenses.
  // Match the native sketch's DISPLAY_SCALE=2 at 64px: the saved source
  // wavelength 5 / distortion 4 correspond to 10px / 8px on this panel.
  float wavelength=mapControl(p[1],2.5f,13.f)*2.f*S;
  float distortion=mapControl(p[2],.5f,9.f)*2.f*S,interference=unit(p[3]);
  float row[64],column[64];
  for(int y=0;y<H;y++)row[y]=silkySin(y*.075f/S+t*.51f);
  for(int x=0;x<W;x++)column[x]=silkyCos(x*.07f/S-t*.37f);
  const float cx=(W-1)*.5f,cy=(H-1)*.5f;
  for(int y=0;y<H;y++)for(int x=0;x<W;x++){
   float r=polarRadius[y*W+x]*H;
   // Per-pixel field evaluation uses the fast LUT: its phase error moves the
   // lattice by <0.1px here. Keep interpolation for the particle trajectories.
   float carrier=fastSinApprox(r/wavelength-t*3.f);
   float secondary=fastSinApprox((x-cx)*.12f/S+(y-cy)*.09f/S+t*1.1f+row[y]*1.4f);
   float lens=(row[y]*column[x]+secondary)*interference;
   float phase=(carrier+lens*.7f)*distortion;
   // Smoothly transfer coverage between the two lattice phases instead of
   // abruptly flipping a whole ring. This is antialiasing, not dithering.
   int ip=floorInt(phase);float fraction=phase-ip;
   fraction=fraction*fraction*(3.f-2.f*fraction);
   float light=((x+y+ip)&1)?fraction:1.f-fraction;
   float sheen=.75f+.25f*fastSinApprox(r*.075f/S-t*.6f+secondary);
   float hue=.69f+.055f*fastSinApprox(t*.22f+r*.075f/S)+lens*.045f;
   remixPixels[y*W+x]=colorAt(pal,hue,light*sheen);
  }
 }else if(id==2){
  // Fixed virtual 60 Hz retains the supplied sketch's spawn/fade behavior at
  // 30/60/120 FPS. Fractional carry avoids rounding the life decay per frame.
  // Radii scale from the source's 22px matrix; all five rings can cross 64px.
  const float scale=min(W,H)/22.f,u=unit(p[0]);
  const float speed=(.008f+.212f*u*u)*scale,thick=mapControl(p[1],.5f,4.2f)*scale;
  const int density=1+p[2]*11/100,decay=1+p[3]*9/100;
  const float maxRadius=mapControl(p[3],36.f,18.f)*scale;
  const uint32_t fade=256-(1+p[3]*47/100);
  // Brightness is already baked into pal. Additive trails must not bypass it.
  int capR=0,capG=0,capB=0;
  for(int i=0;i<256;i++){capR=max(capR,(pal[i]>>11)*2114);capG=max(capG,((pal[i]>>5)&63)*1040);capB=max(capB,(pal[i]&31)*2114);}
  rippleCarry+=elapsed*60.f;int ticks=min(6,(int)(rippleCarry+1e-5f));rippleCarry=max(0.f,rippleCarry-ticks);
  for(int tick=0;tick<ticks;tick++){
   for(int i=0;i<W*H*3;i++)rippleTrail[i]=(uint32_t(rippleTrail[i])*fade)>>8;
   if(randomUnit(rippleSeed)*255.f<density)spawnAdvancedRipple(W,H);
   for(auto&r:advancedRipples)if(r.life>0){
    r.radius+=speed;r.life=max(0.f,r.life-decay-(r.radius>maxRadius?2.f:0.f));
    const float outer=r.radius+thick,inner=max(0.f,r.radius-thick),outer2=outer*outer,inner2=inner*inner;
    const float invThick=1.f/thick,life=r.life/255.f;
    // Keep the original gentle hue spread, now routed through the palette.
    const uint16_t color=pal[r.hue];
    int cr=(color>>11)*2114,cg=((color>>5)&63)*1040,cb=(color&31)*2114;
    for(int y=max(0,(int)(r.y-outer));y<=min(int(H)-1,(int)ceilf(r.y+outer));y++){
     float dy=y-r.y,dy2=dy*dy;
     for(int x=max(0,(int)(r.x-outer));x<=min(int(W)-1,(int)ceilf(r.x+outer));x++){
      float dx=x-r.x,d2=dx*dx+dy2;if(d2<inner2||d2>outer2)continue;
      float soft=max(0.f,1.f-fabsf(fastSqrtApprox(d2)-r.radius)*invThick);
      float amount=soft*soft*life;int q=(y*W+x)*3;
      int rr=int(rippleTrail[q])+int(cr*amount),gg=int(rippleTrail[q+1])+int(cg*amount),bb=int(rippleTrail[q+2])+int(cb*amount);
      // One shared limiter preserves the blended pigment. Independent channel
      // clipping eventually made even a single teal/orange ring turn white.
      float gain=1.f;
      if(rr>capR)gain=min(gain,capR/float(rr));
      if(gg>capG)gain=min(gain,capG/float(gg));
      if(bb>capB)gain=min(gain,capB/float(bb));
      rippleTrail[q]=int(rr*gain);rippleTrail[q+1]=int(gg*gain);rippleTrail[q+2]=int(bb*gain);
     }
    }
   }
  }
  for(int i=0;i<W*H;i++)remixPixels[i]=((min(capR,int(rippleTrail[i*3]))>>11)<<11)|((min(capG,int(rippleTrail[i*3+1]))>>10)<<5)|(min(capB,int(rippleTrail[i*3+2]))>>11);
 }
 for(int y=0;y<H;y++)for(int x=0;x<W;x++)writer(context,x,y,remixPixels[y*W+x]);
}
}
