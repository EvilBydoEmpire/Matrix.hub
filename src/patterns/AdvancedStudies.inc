// Further studies. Abyssal Vortex derives from Topo Vortex, a Patternflow work,
// and is CC-BY-SA-4.0; Interference develops the native Warped Checkerboard, and
// Ripples ports a privately supplied five-ring sketch.
// docs/THIRD_PARTY_NOTICES.md credits them.
namespace {
struct AdvancedRipple {float x,y,radius,life;uint8_t hue;};
AdvancedRipple advancedRipples[5]{};
uint16_t* rippleTrail=nullptr; // RGB in 16-bit channels, as trailAdd keeps them.
float rippleCarry=FIRST_CARRY;
uint32_t rippleSeed=0x71C9AE13;
void resetAdvancedPattern(){
 free(rippleTrail);rippleTrail=nullptr;rippleCarry=FIRST_CARRY;rippleSeed=0x71C9AE13;
 for(auto&r:advancedRipples)r={};
}
void spawnAdvancedRipple(uint16_t W,uint16_t H){
 for(auto&r:advancedRipples)if(r.life<=0){
  r.x=randomUnit(rippleSeed)*(W-1);r.y=randomUnit(rippleSeed)*(H-1);
  r.radius=0;r.life=255;r.hue=64+(uint8_t)(randomUnit(rippleSeed)*10.f);break;
 }
}
void renderAdvancedPattern(uint8_t id,uint16_t W,uint16_t H,float dt,const uint8_t p[4],
 const uint16_t pal[256],PatternflowPixelWriter writer,void* context){
 if(!claimCanvas(id+200,W,H,resetAdvancedPattern))return;
 if(id==2&&!rippleTrail){
  rippleTrail=(uint16_t*)calloc(W*H*3,sizeof(uint16_t));
  if(!rippleTrail)return;
  spawnAdvancedRipple(W,H);
 }
 const float S=min(W,H)/64.f;
 remixClock+=dt*mapControl(p[id==0?1:0],.1f,3.3f);
 const float t=remixClock;
 memset(remixPixels,0,W*H*sizeof(uint16_t));
 if(id==0){ // Abyssal Vortex: dashed concentric strands braid around two living cores.
  // Rings keep at least two pixels between them, and fewer fit smaller panels.
  const int rings=scaledCount(p[0],6,20,3,min(W,H)/64.f);
  const float pitch=max(S,.56f);
  const float twist=unit(p[2])*4.5f,braid=unit(p[3]);
  // Angular harmonics shared by every contour. Each ring has its own travelling
  // phase, radial breathing, toroidal shear and bright moving wave packets.
  constexpr int N=128;float sn[N],cs[N],s2[N],s3[N],s5[N];
  for(int j=0;j<N;j++){float a=j*(2.f*PI/N);sn[j]=smoothSin(a);cs[j]=smoothCos(a);
   s2[j]=smoothSin(a*2.f-t*.73f);s3[j]=smoothSin(a*3.f+t*.41f);s5[j]=smoothSin(a*5.f-t*1.1f);}
  for(int core=0;core<2;core++){
   float dir=core?-1.f:1.f;
   float ox=W*(core?.22f:.62f)+smoothSin(t*.23f+core*PI)*W*.055f*braid;
   float oy=H*(core?.72f:.28f)+smoothCos(t*.31f+core*PI)*H*.055f*braid;
   for(int ring=rings;ring>0;ring--){
    float k=ring/float(rings),base=(2.f+ring*3.6f)*pitch;
    float phase=t*.35f*dir+twist*8.f/(base/pitch+8.f)+smoothSin(t*.37f+k*5.f)*braid*.27f;
    float ca=smoothCos(phase),sa=smoothSin(phase);
    float breath=smoothSin(k*8.f-t*1.6f+core*2.f);
    // About one dash per pixel and a half of circumference.
    int stride=max(ring<4?5:ring<8?3:2,(int)(N*1.6f/(2.f*PI*base)));
    uint16_t color=pal[phaseByte(k*190.f)];
    for(int j=0;j<N;j+=stride){
     float deform=s2[j]*(.11f+.045f*breath)+s3[j]*braid*.06f+s5[j]*braid*.024f;
     float r=base*(1.f+deform)+breath*1.8f*S;
     float ux=cs[j]*ca-sn[j]*sa,uy=sn[j]*ca+cs[j]*sa;
     float x=ox+ux*r+s3[j]*braid*2.f*S,y=oy+uy*r*.92f+s2[j]*2.f*S;
     float packet=.5f+.5f*smoothSin(j*.147f+k*11.f-t*2.1f*dir);
     float light=.64f+.36f*packet*packet;
     remixPoint(x,y,color,light*1.65f);
     // Bright packets add a short tangent glint beside their dash.
     if(packet>.82f&&braid>.05f)remixPoint(x-uy*.7f*S,y+ux*.7f*S,color,braid*.75f);
    }
   }
  }
 }else if(id==1){ // Interference: Warped Checkerboard's one-pixel lattice under three wave lenses.
  // Match the native engine's DISPLAY_SCALE=2 at 64 px: wavelength 5 and
  // distortion 4 become 10 px and 8 px.
  float wavelength=mapControl(p[1],2.5f,13.f)*2.f*S;
  float distortion=mapControl(p[2],.5f,9.f)*2.f*S,interference=unit(p[3]);
  float row[64],column[64];
  for(int y=0;y<H;y++)row[y]=smoothSin(y*.075f/S+t*.51f);
  for(int x=0;x<W;x++)column[x]=smoothCos(x*.07f/S-t*.37f);
  const float cx=(W-1)*.5f,cy=(H-1)*.5f;
  for(int y=0;y<H;y++)for(int x=0;x<W;x++){
   float r=polarRadius[y*W+x]*H;
   // Per-pixel field evaluation uses the fast LUT: its phase error moves the
   // lattice by <0.1px here. Keep interpolation for the particle trajectories.
   float carrier=fastSin(r/wavelength-t*3.f);
   float secondary=fastSin((x-cx)*.12f/S+(y-cy)*.09f/S+t*1.1f+row[y]*1.4f);
   float lens=(row[y]*column[x]+secondary)*interference;
   float phase=(carrier+lens*.7f)*distortion;
   // Coverage moves smoothly between the two lattice phases, so a ring never
   // flips at once.
   int ip=floorInt(phase);float fraction=phase-ip;
   fraction=fraction*fraction*(3.f-2.f*fraction);
   float light=((x+y+ip)&1)?fraction:1.f-fraction;
   float sheen=.75f+.25f*fastSin(r*.075f/S-t*.6f+secondary);
   // Rings sweep from the palette's body through its highlight to its accent.
   float hue=.47f+.25f*fastSin(t*.22f+r*.075f/S)+lens*.045f;
   remixPixels[y*W+x]=paletteColorAt(pal,hue,light*sheen);
  }
 }else if(id==2){
  // Fixed virtual 60 Hz retains the supplied sketch's spawn/fade behaviour at
  // 30/60/120 FPS. Fractional carry avoids rounding the life decay per frame.
  // Radii scale from the source's 22px matrix; all five rings can cross 64px.
  const float scale=min(W,H)/22.f,u=unit(p[0]);
  const float speed=(.008f+.212f*u*u)*scale,thick=mapControl(p[1],.5f,4.2f)*scale;
  const int density=1+p[2]*11/100,decay=1+p[3]*9/100;
  const float maxRadius=mapControl(p[3],36.f,18.f)*scale;
  const uint32_t fade=256-(1+p[3]*47/100);
  // Brightness is already baked into pal. Additive trails must not bypass it.
  const TrailCap cap=trailCap(pal);
  for(int tick=fixedSteps(rippleCarry,dt,60);tick>0;tick--){
   trailFade(rippleTrail,W*H*3,fade);
   if(randomUnit(rippleSeed)*255.f<density)spawnAdvancedRipple(W,H);
   for(auto&r:advancedRipples)if(r.life>0){
    r.radius+=speed;r.life=max(0.f,r.life-decay-(r.radius>maxRadius?2.f:0.f));
    const float outer=r.radius+thick,inner=max(0.f,r.radius-thick),outer2=outer*outer,inner2=inner*inner;
    const float invThick=1.f/thick,life=r.life/255.f;
    // Each ripple takes one of ten entries at the palette's body.
    const uint16_t color=pal[r.hue];
    for(int y=max(0,(int)(r.y-outer));y<=min(int(H)-1,(int)ceilf(r.y+outer));y++){
     float dy=y-r.y,dy2=dy*dy;
     for(int x=max(0,(int)(r.x-outer));x<=min(int(W)-1,(int)ceilf(r.x+outer));x++){
      float dx=x-r.x,d2=dx*dx+dy2;if(d2<inner2||d2>outer2)continue;
      float soft=max(0.f,1.f-fabsf(fastSqrt(d2)-r.radius)*invThick);
      trailAdd(rippleTrail+(y*W+x)*3,color,soft*soft*life,cap);
     }
    }
   }
  }
  for(int i=0;i<W*H;i++)remixPixels[i]=trailColor(rippleTrail+i*3,cap);
 }
 flushCanvas(writer,context);
}
}
