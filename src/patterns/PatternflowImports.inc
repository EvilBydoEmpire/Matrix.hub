// Native ESP32 adaptations of the supplied Patternflow Community patterns.
// SPDX-License-Identifier: CC-BY-SA-4.0
// Authors and exact original JS: patterns/imports/patternflow, THIRD_PARTY_NOTICES.
// Included after the existing LUT/noise helpers. One workspace, never 18 buffers.
namespace {
#if defined(MATRIX_PROFILE_WS2812)
constexpr int KELP_CAP = 1024;
#else
constexpr int KELP_CAP = 4096;
#endif
struct ImportedWorkspace {
  union {
    float field[3][GEOMETRY_PIXELS];
    struct { float x[KELP_CAP],y[KELP_CAP],angle[KELP_CAP]; } kelp;
  };
};
ImportedWorkspace* importedWork=nullptr;
int importedIndex=-1, importedWidth=0, importedHeight=0;
float importedTime=0, importedPhase=0, importedNorm=1, importedX=0,importedY=0;
int importedStage=0, importedTick=0;
uint32_t importedSeed=98712345;
void resetImportedPattern() {
  if(importedWork){free(importedWork);importedWork=nullptr;}
  importedIndex=-1;importedTime=0;importedPhase=0;importedNorm=1;
  importedX=importedY=0;importedStage=importedTick=0;importedSeed=98712345;
}
inline float fastReciprocal(float x){
  if(x==0.f)return 0.f;
  union {float f;uint32_t bits;} v={fabsf(x)};
  v.bits=0x7EF311C3U-v.bits;float r=v.f;
  r*=2.f-fabsf(x)*r;r*=2.f-fabsf(x)*r;r*=2.f-fabsf(x)*r;
  return x<0.f?-r:r;
}
inline float smooth(float a,float b,float x){float v=clamp01((x-a)*fastReciprocal(b-a));return v*v*(3.f-2.f*v);}
inline float mixf(float a,float b,float v){return a+(b-a)*v;}
inline uint16_t packRGB(float r,float g,float b) {
  uint8_t R=(uint8_t)constrain(r,0.f,255.f),G=(uint8_t)constrain(g,0.f,255.f),B=(uint8_t)constrain(b,0.f,255.f);
  return ((R&248)<<8)|((G&252)<<3)|(B>>3);
}
inline float ihash(uint32_t n){n*=2654435761U;n^=n>>15;n*=2246822519U;n^=n>>13;return (n&0xFFFFFF)*(1.f/16777216.f);}
float solitonKink(float v){
  // Smooth Sine-Gordon kink, interpolated at 1/32 spacing rather than two
  // software transcendental calls per pixel. Saturates safely at both ends.
  static uint16_t table[769];static bool ready=false;
  if(!ready){for(int i=0;i<=768;i++)table[i]=(uint16_t)(4.f*atanf(expf(i/32.f-12.f))*(65535.f/(2.f*PI)));ready=true;}
  float position=constrain(v+12.f,0.f,24.f)*32.f;int i=min(767,(int)position);
  return mixf(table[i],table[i+1],position-i)*(2.f*PI/65535.f);
}
inline float segmentDistance(float x,float y,float ax,float ay,float bx,float by,float r0,float r1){
  float dx=bx-ax,dy=by-ay,den=dx*dx+dy*dy;
  float u=den>0?clamp01(((x-ax)*dx+(y-ay)*dy)*fastReciprocal(den)):0;
  return fastHypotApprox(x-ax-u*dx,y-ay-u*dy)-mixf(r0,r1,u);
}
float inkNoise(float x,float y,int octaves) {
  float sum=0,amp=.5f,norm=0;
  for(int i=0;i<octaves;i++){
    int ix=(int)x,iy=(int)y;if(x<ix)ix--;if(y<iy)iy--;
    float fx=x-ix,fy=y-iy,ux=fx*fx*(3-2*fx),uy=fy*fy*(3-2*fy);
    auto hash=[](int a,int b){return ihash(uint32_t(a)*374761393U+uint32_t(b)*668265263U);};
    float v=mixf(mixf(hash(ix,iy),hash(ix+1,iy),ux),mixf(hash(ix,iy+1),hash(ix+1,iy+1),ux),uy);
    sum+=amp*(v*2.f-1.f);norm+=amp;x*=2.03f;y*=2.03f;amp*=.5f;
  }
  return sum*fastReciprocal(max(norm,.001f));
}
// Collatz topology is immutable and lives in flash, not a runtime JS Map.
#include "../generated/CollatzTopology.h"

void renderImportedPattern(uint8_t index,uint16_t W,uint16_t H,float dt,
 const uint8_t p[4],const uint16_t pal[256],const uint16_t rgbMap[4096],
 PatternflowPixelWriter writer,void* context){
  ensureFastSinTable();
  dt=constrain(dt,0.f,.05f);
  if(importedIndex!=index||importedWidth!=W||importedHeight!=H){
    resetImportedPattern();importedIndex=index;importedWidth=W;importedHeight=H;
    // Only simulations need storage; analytic shaders allocate nothing.
    if(index==4||index==6||index==14){
      importedWork=(ImportedWorkspace*)calloc(1,sizeof(ImportedWorkspace));
      if(!importedWork)return;
    }
    if(index==4)for(int y=0;y<H;y++)for(int x=0;x<W;x++){
      const float nx=x*2.f/W-1.f,ny=y*4.f/H-2.f;
      importedWork->field[0][y*W+x]=solitonKink(nx*3.f)*fastSinApprox(ny*2.f);
    }
  }
  if((index==4||index==6||index==14)&&!importedWork)return;
  importedTime+=dt;
  const float t=importedTime,iw=1.f/W,ih=1.f/H,S=min(W,H)/64.f;
  auto rgb=[&](int x,int y,float r,float g,float b){
    if(x<0||y<0||x>=W||y>=H)return;
    int R=constrain((int)r,0,255),G=constrain((int)g,0,255),B=constrain((int)b,0,255);
    writer(context,x,y,rgbMap[((R>>4)<<8)|((G>>4)<<4)|(B>>4)]);
  };
  auto value=[&](int x,int y,float v){writer(context,x,y,pal[(uint8_t)(clamp01(v)*191.f)]);};
  auto hue=[&](int x,int y,float h,float v=1.f){writer(context,x,y,colorAt(pal,h,v));};
  auto line=[&](float ax,float ay,float bx,float by,float h,float v){
    int steps=min(256,max(1,(int)ceilf(max(fabsf(bx-ax),fabsf(by-ay))*1.3f)));
    for(int n=0;n<=steps;n++){float u=n/(float)steps;int x=roundf(mixf(ax,bx,u)),y=roundf(mixf(ay,by,u));if(x>=0&&y>=0&&x<W&&y<H)hue(x,y,h,v);}
  };
  switch(index){
    case 1:{ // Vega444_C @WollyMeister — opaque top layer makes Code 2 unnecessary.
      float grid=roundf(mapControl(p[0],4,16)),bulge=mapControl(p[1],0,1.4f),depth=mapControl(p[2],0,1.5f),phase=t*.45f*min(1.f,depth)+unit(p[3])*PI*2;
      const float cell=1.84f/grid,invCell=1.f/cell;
      static float asinTable[257];static bool asinReady=false;
      if(!asinReady){for(int i=0;i<=256;i++)asinTable[i]=asinf(i/256.f)*(2.f/PI);asinReady=true;}
      for(int y=0;y<H;y++)for(int x=0;x<W;x++){
        float nx=(x+.5f-W*.5f)*2*ih,ny=(y+.5f-H*.5f)*2*ih,r=fastHypotApprox(nx,ny),ratio=r/.92f,scale=1;
        if(ratio<1&&ratio>.0001f){float at=ratio*256;int ai=min(255,(int)at);scale=mixf(ratio,mixf(asinTable[ai],asinTable[ai+1],at-ai),bulge)*fastReciprocal(ratio);}
        float cu=nx*scale*invCell,cv=ny*scale*invCell,iu=roundf(cu),iv=roundf(cv),d=fastHypotApprox(cu-iu,cv-iv);
        float wave=fastSinApprox(r*6-t*3.2f),rc=fastHypotApprox(iu*cell,iv*cell),a=fastAtan2Approx(iv,iu);
        float shift=.45f*fastSinApprox(rc*2.6f+2*a-phase),mR=1-smooth(.44f*(1+.22f*depth*wave)-.055f,.44f*(1+.22f*depth*wave)+.055f,d);
        float mC=1-smooth(.29f*(1+.22f*depth*wave)-.055f,.29f*(1+.22f*depth*wave)+.055f,d);
        float shade=ratio<1?mixf(1,.09f,smooth(0,1,ratio)):.44f,lum=1+.1f*depth*wave;
        float R=ratio<1?shade*.66f:.11f,G=ratio<1?shade*.76f:.44f,B=ratio<1?shade*.93f:.32f;
        R=mixf(R,.14f+shift*.1f,mR);G=mixf(G,.64f,mR);B=mixf(B,.38f-shift*.1f,mR);
        R=mixf(R,.85f,mC);G=mixf(G,.16f+shift*.2f,mC);B=mixf(B,.16f-shift*.2f,mC);
        R=clamp01(R*lum);G=clamp01(G*lum);B=clamp01(B*lum);rgb(x,y,R*R*255,G*G*255,B*B*255);
      }break;
    }
    case 2:{ // Twisted Tunnel @kpow — stable log-indexed nested frames, rotated analytic AA.
      float step=mapControl(p[0],1.2f,1.72f),tilt=mapControl(p[1],0,.5f),speed=mapControl(p[3],.05f,12),lns=logf(step);
      importedPhase+=dt*speed/5.2f;if(importedPhase>1000)importedPhase-=1000;
      float z=40+importedPhase;
      struct Ring{float w,h,ox,oy,c,s;int color,pattern;};Ring ring[64];int count=0;
      int ki=floorf(z-logf(.4f)/lns),ko=ceilf(z-logf(900.f)/lns);
      for(int k=ki;k>=ko&&count<64;k--){int km=(k%1000+1000)%1000;float age=z-k,w=expf((age+.3f*(ihash(km*16+7)*2-1))*lns);
        float ar=(.74f+.42f*((km*7)%5)/5.f)*(1+.18f*(ihash(km*16+2)*2-1));
        float rot=age*tilt+.14f*(ihash(km*16+5)*2-1)+.06f*fastSinApprox(t*.4f+km);
        int ci=ihash(km*16)*5,outer=ihash(((km+999)%1000)*16)*5;if(ci==outer)ci=(ci+1+(int)(ihash(km*16+8)*4))%5;
        ring[count++]={w,w*ar,(fract(km*.61803399f)-.5f)*.22f*w+.06f*w*fastSinApprox(t*.4f+km),
          (fract(km*1.23606798f)-.5f)*.17f*w*ar,fastCosApprox(rot),fastSinApprox(rot),ci,ihash(km*16+12)<.2f?(int)(ihash(km*16+13)*5):-1};
      }
      const float colors[5][3]={{209,20,20},{15,71,140},{242,194,10},{0,0,0},{255,255,255}};
      const float invScale=1.f/S,shift=(unit(p[2])-.5f)*2*PI,cs=fastCosApprox(shift),sn=fastSinApprox(shift),k=(1-cs)/3,plus=k+.57735f*sn,minus=k-.57735f*sn,d=cs+k;
      for(int y=0;y<H;y++)for(int x=0;x<W;x++){float R=0,G=0,B=0,rem=1;
        for(int i=0;i<count&&rem>.002f;i++){const Ring&q=ring[i];float dx=(x+.5f-W*.5f)*invScale-q.ox,dy=(y+.5f-H*.5f)*invScale-q.oy;
          float lx=dx*q.c+dy*q.s,ly=-dx*q.s+dy*q.c,cov=clamp01((q.w*.5f-fabsf(lx))*S+.5f)*clamp01((q.h*.5f-fabsf(ly))*S+.5f);if(cov<=0)continue;
          float ink=0,per=q.w*.1f;if(q.pattern>=0&&per>2){float invPer=fastReciprocal(per),u=lx*invPer,v=ly*invPer,tx=fabsf(fract(u)-.5f),ty=fabsf(fract(v)-.5f);
            ink=q.pattern==0?1-smooth(.24f,.3f,fastHypotApprox(tx,ty)):q.pattern==1?(max(tx,ty)>.37f?1:0):q.pattern==2?(fabsf(fract((u+v)*.7071f)-.5f)<.25f?1:0):q.pattern==3?(((int)floorf(u)^(int)floorf(v))&1):(ty<.22f?1:0);
            ink*=smooth(2,3.5f,per);}
          const float* c=colors[q.color];float blackWhite=q.color==3?255:0,weight=rem*cov;
          R+=weight*mixf(c[0],blackWhite,ink);G+=weight*mixf(c[1],blackWhite,ink);B+=weight*mixf(c[2],blackWhite,ink);rem-=weight;
        }
        // Color knob hue rotates the pigment relationship; original at centre.
        rgb(x,y,d*R+minus*G+plus*B,plus*R+d*G+minus*B,minus*R+plus*G+d*B);
      }break;
    }
    case 4:{ // SolitonLatticeEcho @engmung — actual Sine-Gordon state + hyperbolic echo.
      float* phi=importedWork->field[0],*vel=importedWork->field[1],*echo=importedWork->field[2];
      // Log mapping retains usable low-energy breathers as well as sharp kinks.
      float energy=.3f*expf(unit(p[0])*logf(1108.f/.3f)),pace=mapControl(p[1],.2f,12),count=mapControl(p[2],1,5),decay=mapControl(p[3],.4f,.98f),maximum=.01f;
      const float velocityDecay=powf(.985f,dt*60),echoDecay=powf(decay,dt*60);
      const float shiftX=.35f*fastSinApprox(t*pace*1.3f),shiftY=fmodf(t*pace*.3f,2),kinkScale=count*energy;
      for(int y=0;y<H;y++){int up=y?y-1:H-1,down=y+1==H?0:y+1;float ny=y*2*ih-1;
        for(int x=0;x<W;x++){int left=x?x-1:W-1,right=x+1==W?0:x+1,i=y*W+x;float nx=x*2*iw-1,old=phi[i];
          float lap=phi[y*W+left]+phi[y*W+right]+phi[up*W+x]+phi[down*W+x]-4*old;
          vel[i]=(vel[i]+dt*(.35f*lap-fastSinApprox(old)))*velocityDecay;
          float kx=nx+shiftX,ky=ny-shiftY;
          float next=old+dt*vel[i]+(solitonKink((kx*2-ky*1.5f)*kinkScale)-solitonKink((-kx*2-ky*1.5f)*kinkScale))*.035f*dt*60;
          phi[i]=constrain(next,-1000.f,1000.f);
          float gx=(phi[y*W+right]-phi[y*W+left])*.5f,gy=(phi[down*W+x]-phi[up*W+x])*.5f;
          float u=clamp01(nx*.48f+.02f*fastSinApprox(next*2)+.5f)*(W-1),v=clamp01(ny*.48f+.008f*fastCosApprox(next*2)+.506f)*(H-1);
          int sx=u,sy=v,sx1=min(sx+1,(int)W-1),sy1=min(sy+1,(int)H-1);
          float prev=mixf(mixf(echo[sy*W+sx],echo[sy*W+sx1],u-sx),mixf(echo[sy1*W+sx],echo[sy1*W+sx1],u-sx),v-sy);
          float en=vel[i]*vel[i]+(gx*gx+gy*gy)*.5f+1-fastCosApprox(next);
          echo[i]=min(10000.f,prev*echoDecay+en*.3f);maximum=max(maximum,echo[i]);
        }
      }
      importedNorm=mixf(importedNorm,maximum,1-expf(-dt*5));
      for(int y=0;y<H;y++)for(int x=0;x<W;x++){int i=y*W+x;float v=clamp01(echo[i]/max(.001f,importedNorm)*.7f+fabsf(fastSinApprox(phi[i]*.5f))*.35f);value(x,y,v*v*(3-2*v));}
      break;
    }
    case 5:{ // RippleCellGrid2 @engmung: nearest regular-grid center replaces nine sqrt calls.
      float speed=mapControl(p[0],0,2),spacing=mapControl(p[1],3,6)*2.5f*S,edge=mapControl(p[3],.5f,3)*2.5f*S;
      importedPhase+=dt*speed;
      for(int y=0;y<H;y++)for(int x=0;x<W;x++){float dx=x-W*.5f,dy=y-H*.5f,r=fastHypotApprox(dx,dy),off=unit(p[2])*20*S*fastSinApprox(r*.08f/S-importedPhase*1.5f),scale=r>.001f?(r+off)/r:1;
        float rx=dx*scale,ry=dy*scale,ex=(fract(rx/spacing)-.5f)*spacing,ey=(fract(ry/spacing)-.5f)*spacing,d=fastHypotApprox(ex,ey);
        value(x,y,d<edge?1-d/edge*.9f:.05f);}break;
    }
    case 6:{ // Poolside @engmung — refracted photon accumulation, not a substituted noise shader.
      float* next=importedWork->field[0],*acc=importedWork->field[1];memset(next,0,W*H*sizeof(float));
      float depth=mapControl(p[0],3,30)*S,drift=mapControl(p[1],.2f,4),chop=mapControl(p[2],.4f,2),swell=mapControl(p[3],20,80)*S,k1=2*PI/swell,k2=k1/.57f,k3=k1/.31f;
      for(int y=0;y<H;y++)for(int x=0;x<W;x++){float a=fastCosApprox(k1*(x*.14f+y*.99f)+t*drift*1.9f)*chop*.55f,b=fastCosApprox(k2*(x*-.42f+y*.91f)-t*drift*2.6f)*chop*.4f,c=fastCosApprox(k3*(x*.88f+y*.47f)+t*drift*4.1f)*chop*.26f;
        // Bilinear photon splats remove the source's hard integer jumps without dithering.
        float tx=x+(a*.14f-b*.42f+c*.88f)*depth,ty=y+(a*.99f+b*.91f+c*.47f)*depth;int ix=floorf(tx),iy=floorf(ty);float fx=tx-ix,fy=ty-iy;
        for(int j=0;j<2;j++)for(int i=0;i<2;i++){int xx=((ix+i)%W+W)%W,yy=((iy+j)%H+H)%H;next[yy*W+xx]+=.2f*(i?fx:1-fx)*(j?fy:1-fy);}}
      float blend=1-powf(.45f,dt*60);
      for(int y=0;y<H;y++)for(int x=0;x<W;x++){int i=y*W+x;acc[i]=mixf(acc[i],next[i],blend);float a=acc[i];value(x,y,a>.1f?(a-.1f)*1.6f/(1+a):0);}break;
    }
    case 8:{ // Squircle Spacetime Warp @martinnlopez — native AA, fixed slow drift preserved.
      float morph=unit(p[0]),warp=unit(p[1]),freq=unit(p[2]),phase=unit(p[3]),tt=t*.35f;
      const int tiers[6]={2,3,4,5,6,8};int rows=tiers[min(5,(int)(freq*5.999f))],cols=max(1,(int)roundf(rows*W/(float)H));
      float cw=W/(float)cols,ch=H/(float)rows,lx=W*.5f+fastCosApprox(tt*.65f)*W*.22f,ly=H*.5f+fastSinApprox(tt*.5f)*H*.22f,rad=H*.55f;
      for(int y=0;y<H;y++){float ey=fastSinApprox(y*PI/max(1,(int)H-1));for(int x=0;x<W;x++){
        float eff=warp*ey*fastSinApprox(x*PI/max(1,(int)W-1)),dx=x-lx,dy=y-ly,pinch=1-(1/(1+(dx*dx+dy*dy)/(rad*rad)))*.48f*eff;
        float wx=constrain(lx+dx*pinch+fastSinApprox(y*.09f/S+tt*.8f)*8*S*eff,0.f,W-.001f),wy=constrain(ly+dy*pinch+fastCosApprox(x*.09f/S-tt*.7f)*8*S*eff,0.f,H-.001f);
        int cx=wx/cw,cy=wy/ch;float u=(wx-cx*cw-cw*.5f)/(cw*.5f),v=(wy-cy*ch-ch*.5f)/(ch*.5f),u2=u*u,v2=v*v;
        float circle=fastSqrtApprox(u2+v2),squircle=fastSqrtApprox(fastSqrtApprox(u2*u2+v2*v2)),square=max(fabsf(u),fabsf(v));
        float d=morph<.5f?mixf(circle,squircle,morph*2):mixf(squircle,square,morph*2-1);
        float offset=(((cx+cy)&1)?0:.35f)+(cx+cy)*.105f;float intensity=clamp01(fastSinApprox((d*(1.8f+freq*3.4f)-tt*.9f+offset*phase)*2*PI)*3.8f+.5f);
        intensity*=constrain(1-max(0.f,d-1)*1.6f,.15f,1.f);rgb(x,y,intensity*255,intensity*255,intensity*255+6);
      }}break;
    }
    case 12:{ // Hypno Circles @SimonePDA — travelling intersections and white highlights.
      importedPhase+=dt*mapControl(p[1],.1f,10);float tt=importedPhase,freq=.08f+mapControl(p[2],0,4.9f)*.04f,edge=.1f+unit(p[3])*.4f;
      float ax=W*.3f+fastSinApprox(tt*.7f)*W*.2f,ay=H*.5f+fastCosApprox(tt*.5f)*H*.2f,bx=W*.7f-fastSinApprox(tt*.6f)*W*.2f,by=H*.5f-fastCosApprox(tt*.8f)*H*.2f;
      for(int y=0;y<H;y++)for(int x=0;x<W;x++){float a=fastSinApprox(fastHypotApprox(x-ax,y-ay)*.12f/S-tt*4),b=fastSinApprox(fastHypotApprox(x-bx,y-by)*freq/S+tt*3),energy=(a+b)*.5f,diff=fabsf(a-b);
        if(diff<edge){float v=(1-diff/edge)*255;rgb(x,y,v,v,v);}else hue(x,y,unit(p[0])+energy*.3f+(energy<0?.5f:0),constrain((energy+1)*.5f,.1f,1.f)*(energy<0?.6f:1));}break;
    }
    case 14:{ // Collatz Kelp @engmung — original merged Collatz trie, depth growth + current.
      int seedMax=1600;while(seedMax>300&&COLLATZ_UPTO[seedMax]>KELP_CAP)seedMax--;
      int seeds=roundf(mapControl(p[3],300,seedMax)),N=min((int)COLLATZ_UPTO[seeds],KELP_CAP);float odd=mapControl(p[0],.28f,.8f),even=mapControl(p[2],.15f,.45f),grow=mapControl(p[1],.3f,5),tt=t*(.35f+.5f*grow);
      float* X=importedWork->kelp.x,*Y=importedWork->kelp.y,*A=importedWork->kelp.angle;X[0]=Y[0]=A[0]=0;float sx=0,sy=0;int maxDep=1;
      float sway[256];for(int d=0;d<256;d++)sway[d]=.02f*fastSinApprox(tt*.7f-d*.08f)+.007f*fastSinApprox(tt*1.3f-d*.19f+1.7f);
      for(int i=1;i<N;i++){int parent=COLLATZ_PARENT[i],d=COLLATZ_DEPTH[i];float angle=A[parent]+(COLLATZ_ODD[i]?-odd:even)+sway[min(d,255)];A[i]=angle;
        float step=1.3f/(1+.012f*d);X[i]=X[parent]+step*fastSinApprox(angle);Y[i]=Y[parent]+step*fastCosApprox(angle);sx+=X[i];sy+=Y[i];maxDep=max(maxDep,d);}
      float rot=fastAtan2Approx(sx,sy),cr=fastCosApprox(rot),sr=fastSinApprox(rot),minX=1e9f,maxX=-1e9f,minY=1e9f,maxY=-1e9f;
      for(int i=0;i<N;i++){float x=X[i]*cr-Y[i]*sr,y=X[i]*sr+Y[i]*cr;X[i]=x;Y[i]=y;minX=min(minX,x);maxX=max(maxX,x);minY=min(minY,y);maxY=max(maxY,y);}
      float fsx=(W-4)/max(.001f,maxX-minX),fsy=(H-4)/max(.001f,maxY-minY);fsx=min(fsx,fsy*1.6f);fsy=min(fsy,fsx*1.6f);
      float cx=(minX+maxX)*.5f,cy=(minY+maxY)*.5f,rev=t*(.4f+grow)*maxDep/9;
      for(int i=1;i<N;i++){int d=COLLATZ_DEPTH[i];if(d>rev+1)continue;int parent=COLLATZ_PARENT[i];float growth=min(1.f,rev+1-d),ax=W*.5f+(X[parent]-cx)*fsx,ay=H*.5f-(Y[parent]-cy)*fsy;
        float bx=ax+(X[i]-X[parent])*fsx*growth,by=ay-(Y[i]-Y[parent])*fsy*growth,h=d/(float)maxDep*.749f,sp=max(0.f,fastSinApprox(d*.16f-tt*2.4f));line(ax,ay,bx,by,h,.6f+.4f*sp*sp);}
      break;
    }
    case 16:{ // Bioluminescent Topo Vortex @martinnlopez — original dashed contour strands.
      int rings=roundf(mapControl(p[0],8,24));float speed=mapControl(p[1],.1f,4),twist=mapControl(p[2],0,5),bias=unit(p[3]);
      const float center[2][2]={{.62f,.28f},{.22f,.72f}};
      for(int c=0;c<2;c++)for(int k=rings;k>=0;k--){float base=k*3.6f*S,norm=k/(float)rings,pos=clamp01(norm+(bias-.5f)*.6f);
        int dots=max(6,(int)(2*PI*base/(S*(3.6f+norm*2.8f))));
        for(int j=0;j<dots;j++){float a=j*2*PI/dots,swirl=a+twist*8/(base/S+8)+t*speed*.35f*(c? -1:1),r=base+fastSinApprox(base*.14f/S-t*speed*1.6f+a*2+c*47.13f)*base*.16f+fastCosApprox(a*3+t*speed*.8f)*1.8f*S;
          float px=W*center[c][0]+fastCosApprox(swirl)*r+fastSinApprox(base*.08f/S+t*speed*.6f)*3.2f*S,py=H*center[c][1]+fastSinApprox(swirl)*r*.92f+fastCosApprox(a*2-t*speed*.5f)*3.2f*S;
          int x=floorf(px),y=floorf(py);if(x>=0&&y>=0&&x<W&&y<H)hue(x,y,pos*.749f,1);}
      }break;
    }
  }
}
}
