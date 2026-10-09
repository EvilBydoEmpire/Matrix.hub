// Eight independent native-resolution remixes. Originals remain untouched.
// Patternflow-derived algorithms: CC-BY-SA-4.0; attribution in metadata/notices.
// Shared 2-byte/pixel working surface, allocated only while a remix is active.
// Analytic trails and integrated clocks make motion independent of frame rate.
namespace {
uint16_t* remixPixels=nullptr;
int remixIndex=-1,remixW=0,remixH=0;
float remixClock=0;
void resetRemixPattern(){free(remixPixels);remixPixels=nullptr;remixIndex=-1;remixClock=0;}
inline int floorInt(float v){int n=(int)v;return n-(v<n);}
inline uint8_t phaseByte(float v){return (uint8_t)(int32_t)v;}
inline float triangle(float x){return 1.f-fabsf(fract(x)*2.f-1.f);}
struct RemixPoint{float x,y;};

// Sub-pixel coverage, never ordered/error-diffusion dithering. Lighten blending
// avoids turning overlapping saturated strands into broad white blotches.
void remixInk(int x,int y,uint16_t color,float coverage){
 if(x<0||y<0||x>=remixW||y>=remixH||coverage<=0)return;
 const uint16_t scaled=scale565(color,coverage),old=remixPixels[y*remixW+x];
 remixPixels[y*remixW+x]=(max(old&0xF800,scaled&0xF800))|
   (max(old&0x07E0,scaled&0x07E0))|(max(old&0x001F,scaled&0x001F));
}
void remixPoint(float x,float y,uint16_t color,float power){
 int ix=floorInt(x),iy=floorInt(y);float fx=x-ix,fy=y-iy;
 remixInk(ix,iy,color,power*(1-fx)*(1-fy));remixInk(ix+1,iy,color,power*fx*(1-fy));
 remixInk(ix,iy+1,color,power*(1-fx)*fy);remixInk(ix+1,iy+1,color,power*fx*fy);
}
template<void (*Ink)(int,int,uint16_t,float)=remixInk>
void remixLine(RemixPoint a,RemixPoint b,uint16_t color,float power,float width=1.f){
 // Clip before stepping, including at extreme zoom/tunnel depth.
 float dx=b.x-a.x,dy=b.y-a.y,lo=0,hi=1;
 auto clip=[&](float p,float q){if(fabsf(p)<1e-8f)return q>=0;float u=q/p;
   if(p<0){if(u>hi)return false;lo=max(lo,u);}else{if(u<lo)return false;hi=min(hi,u);}return true;};
 if(!clip(-dx,a.x+2)||!clip(dx,remixW+1-a.x)||!clip(-dy,a.y+2)||!clip(dy,remixH+1-a.y))return;
 b={a.x+hi*dx,a.y+hi*dy};a={a.x+lo*dx,a.y+lo*dy};dx=b.x-a.x;dy=b.y-a.y;
 if(max(fabsf(dx),fabsf(dy))<.3f){
  int ix=floorInt(a.x),iy=floorInt(a.y);float fx=a.x-ix,fy=a.y-iy;
  Ink(ix,iy,color,power*(1-fx)*(1-fy));Ink(ix+1,iy,color,power*fx*(1-fy));
  Ink(ix,iy+1,color,power*(1-fx)*fy);Ink(ix+1,iy+1,color,power*fx*fy);return;
 }
 // Analytic, variable-width Wu rasterization: 2–4 coverage samples per major
 // axis pixel, not five bilinear splats (20 writes) per oversampled point.
 const bool steep=fabsf(dy)>fabsf(dx);
 if(steep){float tmp=a.x;a.x=a.y;a.y=tmp;tmp=b.x;b.x=b.y;b.y=tmp;}
 if(a.x>b.x){RemixPoint tmp=a;a=b;b=tmp;}
 float slope=(b.y-a.y)/(b.x-a.x),half=width*.5f*fastSqrtApprox(1.f+slope*slope);
 int start=floorInt(a.x),end=(int)ceilf(b.x);
 float row=a.y+(start-a.x)*slope;
 for(int major=start;major<=end;major++,row+=slope){
  float fade=power*clamp01(major-a.x+.5f)*clamp01(b.x-major+.5f);
  int top=floorInt(row-half),bottom=(int)ceilf(row+half);
  for(int minor=top;minor<=bottom;minor++){
   float coverage=fade*clamp01(half+.5f-fabsf(minor-row));
   if(steep)Ink(minor,major,color,coverage);else Ink(major,minor,color,coverage);
  }
 }
}
void remixQuad(const RemixPoint p[4],uint16_t color){
 float minimum=p[0].y,maximum=minimum;
 for(int i=1;i<4;i++){minimum=min(minimum,p[i].y);maximum=max(maximum,p[i].y);}
 for(int y=max(0,(int)ceilf(minimum));y<=min(remixH-1,(int)floorf(maximum));y++){
  float left=1e6f,right=-1e6f;
  for(int i=0;i<4;i++){const auto&a=p[i];const auto&b=p[(i+1)&3];
   if((a.y<=y&&b.y>y)||(b.y<=y&&a.y>y)){float x=a.x+(y-a.y)*(b.x-a.x)/(b.y-a.y);left=min(left,x);right=max(right,x);}}
  if(left>right)continue;
  int start=max(0,(int)ceilf(left)),end=min(remixW-1,(int)floorf(right));
  for(int x=start;x<=end;x++)remixPixels[y*remixW+x]=color;
 }
}

void renderRemixPattern(uint8_t id,uint16_t W,uint16_t H,float dt,const uint8_t p[4],
 const uint16_t pal[256],PatternflowPixelWriter writer,void* context){
 if(remixIndex!=id||remixW!=W||remixH!=H){
  resetRemixPattern();remixW=W;remixH=H;remixIndex=id;
  remixPixels=(uint16_t*)calloc(W*H,sizeof(uint16_t));
 }
 if(!remixPixels)return;
 memset(remixPixels,0,W*H*sizeof(uint16_t));
 const int speedParam=id==1?2:id==2?3:id==4?1:0;
 const float speed=mapControl(p[speedParam],.08f,1.8f);
 remixClock+=constrain(dt,0.f,.1f)*speed;
 // No clock wrap: discontinuous modulo breaks independent harmonic phases.
 const float t=remixClock,cx=(W-1)*.5f,cy=(H-1)*.5f,S=H/64.f,ih=1.f/H;
 auto paint=[&](int x,int y,float hue,float light){remixPixels[y*W+x]=colorAt(pal,hue,light);};
 auto rotated=[&](float x,float y,float cs,float sn,float ox,float oy){return RemixPoint{ox+x*cs-y*sn,oy+x*sn+y*cs};};
 switch(id){
 case 0:{ // Whitney: differential harmonic canon, satin trails, orbiting pearls.
  const int voices=6+p[1]*18/100,tails=3+p[3]*23/100;
  const float radius=H*mapControl(p[2],.18f,.47f),span=mapControl(p[3],.015f,.33f);
  RemixPoint last={cx,cy};
  for(int i=0;i<voices;i++){
   float k=(i+1.f)/voices,r=radius*(.18f+.82f*k),freq=1.f+i*.1618f;
   const uint16_t color=pal[phaseByte(k*220.f+t*2.f)];RemixPoint prev{};
   for(int j=tails;j>=0;j--){
    float age=j/(float)tails,tt=t-age*span,phase=tt*freq*2.f*PI+i*.55f;
    float bend=tt*.381966f+i*.1f;
    RemixPoint q={cx+r*(fastCosApprox(phase)*.87f+fastSinApprox(bend)*.12f),
      cy+r*(fastSinApprox(phase*1.618034f)*.78f+fastCosApprox(bend)*.12f)};
    const float alpha=(1-age)*(1-age)*.92f+.025f;
    if(j<tails)remixLine(prev,q,color,alpha);prev=q;
    if(j==0){if(i)remixLine(last,q,color,.14f);last=q;
     remixPoint(q.x,q.y,color,1.5f);remixPoint(q.x-.35f,q.y-.35f,pal[(uint8_t)(k*220+14)],.8f);}
   }
  }break;
 }
 case 1:{ // Chladni: separable resonant modes, analytic antialiasing of nodes.
  const float n=1.f+roundf(unit(p[0])*6.f);
  float m=2.f+roundf(unit(p[1])*8.f);if(m==n)m+=1.f;
  const float nextN=n+1.f,nextM=m+2.f+(n+1.f==m+2.f?1.f:0.f);
  const float morph=.5f+.5f*fastSinApprox(t*.72f),blend=morph*morph*(3.f-2.f*morph);
  const float linewidth=mapControl(p[3],.6f,2.4f)*ih;
  float a[64],b[64],c[64],d[64],e[64],f[64],g[64],h[64];
  for(int x=0;x<W;x++){float u=(x-cx)*2.f*ih;
   a[x]=fastCosApprox(n*PI*u);b[x]=fastCosApprox(m*PI*u);c[x]=fastCosApprox(nextN*PI*u);g[x]=fastCosApprox(nextM*PI*u);}
  for(int y=0;y<H;y++){float v=(y-cy)*2.f*ih;
   d[y]=fastCosApprox(n*PI*v);e[y]=fastCosApprox(m*PI*v);f[y]=fastCosApprox(nextN*PI*v);h[y]=fastCosApprox(nextM*PI*v);}
  const float inverseWidth=1.f/(linewidth*(n+m)*2.f+.04f);
  for(int y=0;y<H;y++)for(int x=0;x<W;x++){
   float first=a[x]*e[y]-b[x]*d[y],second=c[x]*h[y]-g[x]*f[y];
   float field=first+(second-first)*blend;
   float ridge=clamp01(1.f-fabsf(field)*inverseWidth);ridge*=ridge;
   float energy=.7f+.3f*fastSinApprox(polarRadius[y*W+x]*12.f-t*1.8f);
   paint(x,y,.12f+polarRadius[y*W+x]*.8f+blend*.13f,.025f+ridge*(.64f+.3f*energy));
  }break;
 }
 case 3:{ // Checkerboard: continuous fabric coordinates, derivative-width AA.
  const float cells=mapControl(p[1],16.f,3.f),warp=unit(p[2]),fold=unit(p[3]);
  float row[64],col[64];
  for(int y=0;y<H;y++)row[y]=fastSinApprox((y-cy)*ih*7.f-t*.9f);
  for(int x=0;x<W;x++)col[x]=fastSinApprox((x-cx)*ih*6.f+t*.65f);
  const float edge=min(.4f,cells*ih*.65f),invEdge=1.f/max(.01f,edge);
  for(int y=0;y<H;y++)for(int x=0;x<W;x++){
   float X=(x-cx)*ih,Y=(y-cy)*ih,r=polarRadius[y*W+x];
   float wave=fastSinApprox(r*13.f-t*1.7f);
   float u=(X+row[y]*warp*.17f+wave*warp*.10f)*cells+t*.18f;
   float v=(Y+col[x]*warp*.15f-wave*warp*.08f)*cells-t*.11f;
   int a=floorInt(u),b=floorInt(v);float fu=u-a,fv=v-b;
   float eu=min(fu,1-fu)*invEdge,ev=min(fv,1-fv)*invEdge;
   float cover=clamp01(min(eu,ev));
   float highlight=.65f+fold*.25f*(row[y]*col[x]+wave*.45f);
   float light=((a+b)&1)?(.035f+.065f*fold):(.22f+cover*.73f)*highlight;
   paint(x,y,.1f+Y*.17f+wave*.12f+t*.015f,light);
  }break;
 }
 case 7:{ // Squares: continuous perspective acceleration + layered corkscrew.
  int count=5+p[1]*15/100;float twist=unit(p[2]),weight=mapControl(p[3],.8f,2.8f);
  float progress=t*.6f,phase=fract(progress),serial=floorf(progress);
  for(int i=0;i<count;i++){
   float depth=(i+phase)/count;
   float size=H*(.016f+depth*depth*1.04f),angle=twist*(depth*depth*2.8f+fastSinApprox(t*.24f)*.45f);
   float ox=cx+fastSinApprox(t*.39f-depth*1.8f)*H*.075f*twist;
   float oy=cy+fastCosApprox(t*.31f-depth*1.3f)*H*.075f*twist;
   float cs=fastCosApprox(angle),sn=fastSinApprox(angle);
   RemixPoint q[4]={rotated(-size,-size,cs,sn,ox,oy),rotated(size,-size,cs,sn,ox,oy),
    rotated(size,size,cs,sn,ox,oy),rotated(-size,size,cs,sn,ox,oy)};
   float light=clamp01(depth*5.f)*(.5f+.5f*depth);
   uint16_t color=pal[(uint8_t)((int)((serial-i)*27.f)&255)];
   for(int e=0;e<4;e++)remixLine(q[e],q[(e+1)&3],color,light,weight);
  }break;
 }
 }
 for(int y=0;y<H;y++)for(int x=0;x<W;x++)writer(context,x,y,remixPixels[y*W+x]);
}
} // namespace
