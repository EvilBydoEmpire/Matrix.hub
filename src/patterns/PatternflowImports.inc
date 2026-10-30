// Adaptations of Patternflow Community works, all CC-BY-SA-4.0. Sources are in
// patterns/imports/patternflow and credits in docs/THIRD_PARTY_NOTICES.md.
// SPDX-License-Identifier: CC-BY-SA-4.0
// The simulations share one workspace, allocated while one of them plays.
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
// Rate controls integrate into importedPhase and importedReveal, so moving one
// never jumps the motion.
float importedTime=0, importedPhase=0, importedReveal=0, importedNorm=1, importedCarry=FIRST_CARRY;
void resetImportedPattern() {
  if(importedWork){free(importedWork);importedWork=nullptr;}
  importedIndex=-1;importedTime=0;importedPhase=0;importedReveal=0;importedNorm=1;importedCarry=FIRST_CARRY;
}
inline float smooth(float a,float b,float x){float v=clamp01((x-a)*fastReciprocal(b-a));return v*v*(3.f-2.f*v);}
inline float mixf(float a,float b,float v){return a+(b-a)*v;}
float solitonKink(float v){
  // Smooth Sine-Gordon kink, interpolated from a table at 1/32 spacing and
  // saturating at both ends.
  static uint16_t table[769];static bool ready=false;
  if(!ready){for(int i=0;i<=768;i++)table[i]=(uint16_t)(4.f*atanf(expf(i/32.f-12.f))*(65535.f/(2.f*PI)));ready=true;}
  float position=constrain(v+12.f,0.f,24.f)*32.f;int i=min(767,(int)position);
  return mixf(table[i],table[i+1],position-i)*(2.f*PI/65535.f);
}
// The merged Collatz tree, precomputed into flash.
#include "../generated/CollatzTopology.h"

void renderImportedPattern(uint8_t index,uint16_t W,uint16_t H,float dt,
 const uint8_t p[4],const uint16_t pal[256],const SourceColors& sourceColors,
 PatternflowPixelWriter writer,void* context){
  if(importedIndex!=index||importedWidth!=W||importedHeight!=H){
    resetImportedPattern();
    // Only simulations need storage; analytic shaders allocate nothing. A failed
    // allocation records no owner, so the next frame tries again.
    if(index==4||index==6||index==14){
      importedWork=(ImportedWorkspace*)calloc(1,sizeof(ImportedWorkspace));
      if(!importedWork)return;
    }
    importedIndex=index;importedWidth=W;importedHeight=H;
    if(index==4)for(int y=0;y<H;y++)for(int x=0;x<W;x++){
      const float nx=(x+.5f)*2.f/W-1.f,ny=(y+.5f)*4.f/H-2.f;
      importedWork->field[0][y*W+x]=solitonKink(nx*3.f)*fastSin(ny*2.f);
    }
  }
  importedTime+=dt;
  const float t=importedTime,iw=1.f/W,ih=1.f/H,S=min(W,H)/64.f;
  auto rgb=[&](int x,int y,float r,float g,float b) PIXEL_INLINE {
    if(x>=0&&y>=0&&x<W&&y<H)writer(context,x,y,sourceColor(sourceColors,(int)r,(int)g,(int)b));
  };
  auto value=[&](int x,int y,float v) PIXEL_INLINE {writer(context,x,y,pal[(uint8_t)(clamp01(v)*191.f)]);};
  auto hue=[&](int x,int y,float h,float v=1.f) PIXEL_INLINE {writer(context,x,y,paletteColorAt(pal,h,v));};
  switch(index){
    case 1:{ // Vega444_C @WollyMeister: its opaque top layer, a lens over a grid of rings.
      float grid=roundf(mapControl(p[0],4,max(5.f,min(16.f,min(W,H)*.25f)))),bulge=mapControl(p[1],0,1.4f),depth=mapControl(p[2],0,1.5f);
      importedPhase+=dt*.45f*min(1.f,depth);const float phase=importedPhase+unit(p[3])*PI*2;
      const float cell=1.84f/grid,invCell=1.f/cell;
      static float asinTable[257];static bool asinReady=false;
      if(!asinReady){for(int i=0;i<=256;i++)asinTable[i]=asinf(i/256.f)*(2.f/PI);asinReady=true;}
      // Each cell's colour shift, once per frame; a cell beyond the table is computed in place.
      constexpr int REACH=11;float cellShift[2*REACH+1][2*REACH+1];
      const int reach=min(REACH,(int)ceilf(invCell)+1);
      for(int v=-reach;v<=reach;v++)for(int u=-reach;u<=reach;u++)
        cellShift[v+REACH][u+REACH]=.45f*fastSin(fastHypot(u*cell,v*cell)*2.6f+2*fastAtan2(v,u)-phase);
      for(int y=0;y<H;y++)for(int x=0;x<W;x++){
        float nx=(x+.5f-W*.5f)*2*ih,ny=(y+.5f-H*.5f)*2*ih,r=fastHypot(nx,ny),ratio=r*(1/.92f),scale=1;
        if(ratio<1&&ratio>.0001f){float at=ratio*256;int ai=min(255,(int)at);scale=mixf(ratio,mixf(asinTable[ai],asinTable[ai+1],at-ai),bulge)*fastReciprocal(ratio);}
        const float cu=nx*scale*invCell,cv=ny*scale*invCell;const int ku=floorInt(cu+.5f),kv=floorInt(cv+.5f);
        const float iu=ku,iv=kv,d=fastHypot(cu-iu,cv-iv),wave=fastSin(r*6-t*3.2f);
        const float shift=abs(ku)<=reach&&abs(kv)<=reach?cellShift[kv+REACH][ku+REACH]:.45f*fastSin(fastHypot(iu*cell,iv*cell)*2.6f+2*fastAtan2(iv,iu)-phase);
        // Ring and centre masks: smoothsteps .11 wide around radii that breathe with the wave.
        const float breathe=1+.22f*depth*wave;
        const float ringStep=clamp01((d-(.44f*breathe-.055f))*(1/.11f)),centreStep=clamp01((d-(.29f*breathe-.055f))*(1/.11f));
        float mR=1-ringStep*ringStep*(3.f-2.f*ringStep),mC=1-centreStep*centreStep*(3.f-2.f*centreStep);
        const float edge=clamp01(ratio);
        float shade=ratio<1?mixf(1,.09f,edge*edge*(3.f-2.f*edge)):.44f,lum=1+.1f*depth*wave;
        float R=ratio<1?shade*.66f:.11f,G=ratio<1?shade*.76f:.44f,B=ratio<1?shade*.93f:.32f;
        R=mixf(R,.14f+shift*.1f,mR);G=mixf(G,.64f,mR);B=mixf(B,.38f-shift*.1f,mR);
        R=mixf(R,.85f,mC);G=mixf(G,.16f+shift*.2f,mC);B=mixf(B,.16f-shift*.2f,mC);
        R=clamp01(R*lum);G=clamp01(G*lum);B=clamp01(B*lum);rgb(x,y,R*R*255,G*G*255,B*B*255);
      }break;
    }
    case 2:{ // Twisted Tunnel @kpow: nested log-indexed frames, rotated and antialiased.
      float step=mapControl(p[0],1.2f,1.72f),tilt=mapControl(p[1],0,.5f),speed=mapControl(p[3],.05f,12),lns=logf(step);
      importedPhase+=dt*speed/5.2f;if(importedPhase>1000)importedPhase-=1000;
      float z=40+importedPhase;
      struct Ring{float w,h,ox,oy,c,s;int color,pattern;};Ring ring[64];int count=0;
      int ki=floorf(z-logf(.4f)/lns),ko=ceilf(z-logf(900.f)/lns);
      for(int k=ki;k>=ko&&count<64;k--){int km=(k%1000+1000)%1000;float age=z-k,w=expf((age+.3f*(hashUnit(km*16+7)*2-1))*lns);
        float ar=(.74f+.42f*((km*7)%5)/5.f)*(1+.18f*(hashUnit(km*16+2)*2-1));
        float rot=age*tilt+.14f*(hashUnit(km*16+5)*2-1)+.06f*fastSin(t*.4f+km);
        int ci=hashUnit(km*16)*5,outer=hashUnit(((km+999)%1000)*16)*5;if(ci==outer)ci=(ci+1+(int)(hashUnit(km*16+8)*4))%5;
        ring[count++]={w,w*ar,(fract(km*.61803399f)-.5f)*.22f*w+.06f*w*fastSin(t*.4f+km),
          (fract(km*1.23606798f)-.5f)*.17f*w*ar,fastCos(rot),fastSin(rot),ci,hashUnit(km*16+12)<.2f?(int)(hashUnit(km*16+13)*5):-1};
      }
      const float colors[5][3]={{209,20,20},{15,71,140},{242,194,10},{0,0,0},{255,255,255}};
      const float invScale=1.f/S;
      for(int y=0;y<H;y++)for(int x=0;x<W;x++){float R=0,G=0,B=0,rem=1;
        for(int i=0;i<count&&rem>.002f;i++){const Ring&q=ring[i];float dx=(x+.5f-W*.5f)*invScale-q.ox,dy=(y+.5f-H*.5f)*invScale-q.oy;
          float lx=dx*q.c+dy*q.s,ly=-dx*q.s+dy*q.c,cov=clamp01((q.w*.5f-fabsf(lx))*S+.5f)*clamp01((q.h*.5f-fabsf(ly))*S+.5f);if(cov<=0)continue;
          float ink=0,per=q.w*.1f;if(q.pattern>=0&&per>2){float invPer=fastReciprocal(per),u=lx*invPer,v=ly*invPer,tx=fabsf(fract(u)-.5f),ty=fabsf(fract(v)-.5f);
            ink=q.pattern==0?1-smooth(.24f,.3f,fastHypot(tx,ty)):q.pattern==1?(max(tx,ty)>.37f?1:0):q.pattern==2?(fabsf(fract((u+v)*.7071f)-.5f)<.25f?1:0):q.pattern==3?(((int)floorf(u)^(int)floorf(v))&1):(ty<.22f?1:0);
            ink*=smooth(2,3.5f,per);}
          const float* c=colors[q.color];float blackWhite=q.color==3?255:0,weight=rem*cov;
          R+=weight*mixf(c[0],blackWhite,ink);G+=weight*mixf(c[1],blackWhite,ink);B+=weight*mixf(c[2],blackWhite,ink);rem-=weight;
        }
        rgb(x,y,R,G,B);
      }break;
    }
    case 4:{ // SolitonLatticeEcho @engmung: a Sine-Gordon field with a hyperbolic echo.
      float* phi=importedWork->field[0],*vel=importedWork->field[1],*echo=importedWork->field[2];
      // Log mapping retains usable low-energy breathers as well as sharp kinks.
      float energy=.3f*expf(unit(p[0])*logf(1108.f/.3f)),pace=mapControl(p[1],.2f,12),count=mapControl(p[2],1,5),decay=mapControl(p[3],.4f,.98f);
      const float kinkScale=count*energy,step=1.f/60;
      // The field integrates at a fixed 60 Hz, so every frame rate sees the same waves.
      for(int tick=fixedSteps(importedCarry,dt,60);tick>0;tick--){
        importedPhase+=step*pace;float maximum=.01f;
        const float shiftX=.35f*fastSin(importedPhase*1.3f),shiftY=fmodf(importedPhase*.3f,2);
        for(int y=0;y<H;y++){int up=y?y-1:H-1,down=y+1==H?0:y+1;float ny=(y+.5f)*2*ih-1;
          for(int x=0;x<W;x++){int left=x?x-1:W-1,right=x+1==W?0:x+1,i=y*W+x;float nx=(x+.5f)*2*iw-1,old=phi[i];
            float lap=phi[y*W+left]+phi[y*W+right]+phi[up*W+x]+phi[down*W+x]-4*old;
            vel[i]=(vel[i]+step*(.35f*lap-fastSin(old)))*.985f;
            float kx=nx+shiftX,ky=ny-shiftY;
            float next=old+step*vel[i]+(solitonKink((kx*2-ky*1.5f)*kinkScale)-solitonKink((-kx*2-ky*1.5f)*kinkScale))*.035f;
            phi[i]=constrain(next,-1000.f,1000.f);
            float gx=(phi[y*W+right]-phi[y*W+left])*.5f,gy=(phi[down*W+x]-phi[up*W+x])*.5f;
            float u=constrain(clamp01(nx*.48f+.02f*fastSin(next*2)+.5f)*W-.5f,0.f,W-1.f),v=constrain(clamp01(ny*.48f+.008f*fastCos(next*2)+.506f)*H-.5f,0.f,H-1.f);
            int sx=u,sy=v,sx1=min(sx+1,(int)W-1),sy1=min(sy+1,(int)H-1);
            float prev=mixf(mixf(echo[sy*W+sx],echo[sy*W+sx1],u-sx),mixf(echo[sy1*W+sx],echo[sy1*W+sx1],u-sx),v-sy);
            float en=vel[i]*vel[i]+(gx*gx+gy*gy)*.5f+1-fastCos(next);
            echo[i]=min(10000.f,prev*decay+en*.3f);maximum=max(maximum,echo[i]);
          }
        }
        importedNorm=mixf(importedNorm,maximum,1-expf(-5*step));
      }
      for(int y=0;y<H;y++)for(int x=0;x<W;x++){int i=y*W+x;float v=clamp01(echo[i]/max(.001f,importedNorm)*.7f+fabsf(fastSin(phi[i]*.5f))*.35f);value(x,y,v*v*(3-2*v));}
      break;
    }
    case 5:{ // RippleCellGrid2 @engmung: dots at the nearest centre of a rippling grid.
      float speed=mapControl(p[0],0,2),spacing=mapControl(p[1],3,6)*2.5f*S,edge=mapControl(p[3],.5f,3)*2.5f*S;
      importedPhase+=dt*speed;
      const float ripple=unit(p[2])*20*S,frequency=.08f/S,wave=importedPhase*1.5f,inverseSpacing=1.f/spacing,shade=.9f/edge;
      for(int y=0;y<H;y++)for(int x=0;x<W;x++){float dx=x-(W-1)*.5f,dy=y-(H-1)*.5f,r=fastHypot(dx,dy),off=ripple*fastSin(r*frequency-wave),scale=r>.001f?(r+off)*fastReciprocal(r):1;
        float rx=dx*scale,ry=dy*scale,ex=(fract(rx*inverseSpacing)-.5f)*spacing,ey=(fract(ry*inverseSpacing)-.5f)*spacing,d=fastHypot(ex,ey);
        value(x,y,d<edge?1-d*shade:.05f);}break;
    }
    case 6:{ // Poolside @engmung: light refracted by the waves gathers on the pool floor.
      float* next=importedWork->field[0],*acc=importedWork->field[1];
      float depth=mapControl(p[0],3,30)*S,drift=mapControl(p[1],.2f,4),chop=mapControl(p[2],.4f,2),swell=mapControl(p[3],20,80)*S,k1=2*PI/swell,k2=k1/.57f,k3=k1/.31f;
      // Light accumulates at a fixed 60 Hz, so every frame rate averages the same samples.
      for(int tick=fixedSteps(importedCarry,dt,60);tick>0;tick--){
        importedPhase+=drift*(1.f/60);const float tt=importedPhase;memset(next,0,W*H*sizeof(float));
        for(int y=0;y<H;y++)for(int x=0;x<W;x++){float a=fastCos(k1*(x*.14f+y*.99f)+tt*1.9f)*chop*.55f,b=fastCos(k2*(x*-.42f+y*.91f)-tt*2.6f)*chop*.4f,c=fastCos(k3*(x*.88f+y*.47f)+tt*4.1f)*chop*.26f;
          // Each photon splats bilinearly over four pixels, so caustics move smoothly;
          // refraction moves it less than a panel, so one wrap keeps it on the panel.
          float tx=x+(a*.14f-b*.42f+c*.88f)*depth,ty=y+(a*.99f+b*.91f+c*.47f)*depth;int ix=floorInt(tx),iy=floorInt(ty);float fx=tx-ix,fy=ty-iy;
          for(int j=0;j<2;j++)for(int i=0;i<2;i++){int xx=ix+i,yy=iy+j;xx+=xx<0?W:xx>=W?-W:0;yy+=yy<0?H:yy>=H?-H:0;next[yy*W+xx]+=.2f*(i?fx:1-fx)*(j?fy:1-fy);}}
        for(int i=0;i<W*H;i++)acc[i]=mixf(acc[i],next[i],.55f);
      }
      for(int y=0;y<H;y++)for(int x=0;x<W;x++){float a=acc[y*W+x];value(x,y,a>.1f?(a-.1f)*1.6f*fastReciprocal(1+a):0);}break;
    }
    case 8:{ // Squircle Spacetime Warp @martinnlopez: antialiased cells under a slowly drifting lens.
      float morph=unit(p[0]),warp=unit(p[1]),freq=unit(p[2]),phase=unit(p[3]),tt=t*.35f;
      const int tiers[6]={2,3,4,5,6,8};int rows=tiers[min(5,(int)(freq*5.999f))],cols=max(1,(int)roundf(rows*W/(float)H));
      float cw=W/(float)cols,ch=H/(float)rows,lx=W*.5f+fastCos(tt*.65f)*W*.22f,ly=H*.5f+fastSin(tt*.5f)*H*.22f,rad=H*.55f;
      const float invRad2=1.f/(rad*rad),invCw=1.f/cw,invCh=1.f/ch,frequency=.09f/S,sway=8*S;
      // Fewer rings fit the smaller cells of a smaller panel, so they stay about two pixels apart.
      const float ringsPerCell=(1.8f+freq*3.4f)*min(1.f,sqrtf(S));
      float edgeX[64],swayY[64];
      for(int x=0;x<W;x++){edgeX[x]=fastSin(x*PI/max(1,(int)W-1));swayY[x]=fastCos(x*frequency-tt*.7f)*sway;}
      for(int y=0;y<H;y++){const float ey=fastSin(y*PI/max(1,(int)H-1)),swayX=fastSin(y*frequency+tt*.8f)*sway;for(int x=0;x<W;x++){
        float eff=warp*ey*edgeX[x],dx=x+.5f-lx,dy=y+.5f-ly,pinch=1-fastReciprocal(1+(dx*dx+dy*dy)*invRad2)*.48f*eff;
        float wx=constrain(lx+dx*pinch+swayX*eff,0.f,W-.001f),wy=constrain(ly+dy*pinch+swayY[x]*eff,0.f,H-.001f);
        int cx=wx*invCw,cy=wy*invCh;float u=(wx-cx*cw)*(2*invCw)-1,v=(wy-cy*ch)*(2*invCh)-1,u2=u*u,v2=v*v;
        float circle=fastSqrt(u2+v2),squircle=fastSqrt(fastSqrt(u2*u2+v2*v2)),square=max(fabsf(u),fabsf(v));
        float d=morph<.5f?mixf(circle,squircle,morph*2):mixf(squircle,square,morph*2-1);
        float offset=(((cx+cy)&1)?0:.35f)+(cx+cy)*.105f;float intensity=clamp01(fastSin((d*ringsPerCell-tt*.9f+offset*phase)*2*PI)*3.8f+.5f);
        intensity*=constrain(1-max(0.f,d-1)*1.6f,.15f,1.f);rgb(x,y,intensity*255,intensity*255,intensity*255+6);
      }}break;
    }
    case 12:{ // Hypno Circles @SimonePDA: travelling intersections and white highlights.
      importedPhase+=dt*mapControl(p[1],.1f,10);float tt=importedPhase,freq=.08f+mapControl(p[2],0,4.9f)*.04f,edge=.1f+unit(p[3])*.4f;
      const float mx=(W-1)*.5f,my=(H-1)*.5f;float ax=mx-W*.2f+fastSin(tt*.7f)*W*.2f,ay=my+fastCos(tt*.5f)*H*.2f,bx=mx+W*.2f-fastSin(tt*.6f)*W*.2f,by=my-fastCos(tt*.8f)*H*.2f;
      for(int y=0;y<H;y++)for(int x=0;x<W;x++){float a=fastSin(fastHypot(x-ax,y-ay)*.12f/S-tt*4),b=fastSin(fastHypot(x-bx,y-by)*freq/S+tt*3),energy=(a+b)*.5f,diff=fabsf(a-b);
        // Crests run from the palette's body to its highlight, troughs dimmer from highlight to accent.
        if(diff<edge){float v=(1-diff/edge)*255;rgb(x,y,v,v,v);}else hue(x,y,unit(p[0])+.25f+energy*.25f+(energy<0?.5f:0),constrain((energy+1)*.5f,.1f,1.f)*(energy<0?.6f:1));}break;
    }
    case 14:{ // Collatz Kelp @engmung: the merged Collatz tree grows by depth and sways in a current.
      int seedMax=1600;while(seedMax>300&&COLLATZ_UPTO[seedMax]>KELP_CAP)seedMax--;
      int seeds=roundf(mapControl(p[3],300,seedMax)),N=min((int)COLLATZ_UPTO[seeds],KELP_CAP);float odd=mapControl(p[0],.28f,.8f),even=mapControl(p[2],.15f,.45f),grow=mapControl(p[1],.3f,5);
      // importedReveal is the grown share of the tree's depth.
      importedPhase+=dt*(.35f+.5f*grow);importedReveal+=dt*(.4f+grow)/9;const float tt=importedPhase;
      float* X=importedWork->kelp.x,*Y=importedWork->kelp.y,*A=importedWork->kelp.angle;X[0]=Y[0]=A[0]=0;float sx=0,sy=0;int maxDep=1;
      float sway[256];for(int d=0;d<256;d++)sway[d]=.02f*fastSin(tt*.7f-d*.08f)+.007f*fastSin(tt*1.3f-d*.19f+1.7f);
      for(int i=1;i<N;i++){int parent=COLLATZ_PARENT[i],d=COLLATZ_DEPTH[i];float angle=A[parent]+(COLLATZ_ODD[i]?-odd:even)+sway[min(d,255)];A[i]=angle;
        float step=1.3f/(1+.012f*d);X[i]=X[parent]+step*fastSin(angle);Y[i]=Y[parent]+step*fastCos(angle);sx+=X[i];sy+=Y[i];maxDep=max(maxDep,d);}
      float rot=fastAtan2(sx,sy),cr=fastCos(rot),sr=fastSin(rot),minX=1e9f,maxX=-1e9f,minY=1e9f,maxY=-1e9f;
      for(int i=0;i<N;i++){float x=X[i]*cr-Y[i]*sr,y=X[i]*sr+Y[i]*cr;X[i]=x;Y[i]=y;minX=min(minX,x);maxX=max(maxX,x);minY=min(minY,y);maxY=max(maxY,y);}
      float fsx=(W-4)/max(.001f,maxX-minX),fsy=(H-4)/max(.001f,maxY-minY);fsx=min(fsx,fsy*1.6f);fsy=min(fsy,fsx*1.6f);
      float cx=(minX+maxX)*.5f,cy=(minY+maxY)*.5f,rev=importedReveal*maxDep;
      for(int i=1;i<N;i++){int d=COLLATZ_DEPTH[i];if(d>rev+1)continue;int parent=COLLATZ_PARENT[i];float growth=min(1.f,rev+1-d),ax=(W-1)*.5f+(X[parent]-cx)*fsx,ay=(H-1)*.5f-(Y[parent]-cy)*fsy;
        float bx=ax+(X[i]-X[parent])*fsx*growth,by=ay-(Y[i]-Y[parent])*fsy*growth,h=d/(float)maxDep*.749f,sp=max(0.f,fastSin(d*.16f-tt*2.4f));paletteLine(writer,context,W,H,pal,ax,ay,bx,by,h,h,.6f+.4f*sp*sp);}
      break;
    }
    case 16:{ // Bioluminescent Topo Vortex @martinnlopez: dashed contour strands around two centres.
      // Fewer rings fit smaller panels. Below 64 pixels rings keep three pixels
      // between them and their dashes close up, so each still reads as a strand.
      const int rings=scaledCount(p[0],8,24,3,S);float twist=mapControl(p[2],0,5),bias=unit(p[3]);
      importedPhase+=dt*mapControl(p[1],.1f,4);const float tt=importedPhase;
      const float center[2][2]={{.62f,.28f},{.22f,.72f}};
      const float pitch=max(S,.84f);
      for(int c=0;c<2;c++)for(int k=rings;k>=0;k--){
        // A ring's radius at 64 pixels sets its motion at any size.
        const float ring=k*3.6f,base=ring*pitch,norm=k/(float)rings,pos=clamp01(norm+(bias-.5f)*.6f);
        int dots=max(6,(int)(2*PI*base/max(1.2f,S*(3.6f+norm*2.8f))));
        for(int j=0;j<dots;j++){float a=j*2*PI/dots,swirl=a+twist*8/(ring+8)+tt*.35f*(c? -1:1),r=base+fastSin(ring*.14f-tt*1.6f+a*2+c*47.13f)*base*.16f+fastCos(a*3+tt*.8f)*1.8f*S;
          float px=W*center[c][0]+fastCos(swirl)*r+fastSin(ring*.08f+tt*.6f)*3.2f*S,py=H*center[c][1]+fastSin(swirl)*r*.92f+fastCos(a*2-tt*.5f)*3.2f*S;
          int x=floorf(px),y=floorf(py);if(x>=0&&y>=0&&x<W&&y<H)hue(x,y,pos*.749f,1);}
      }break;
    }
  }
}
}
