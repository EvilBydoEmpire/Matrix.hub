// Eight of this project's own studies. The last three interpret differential
// motion and visual counterpoint from John Whitney's work. They draw on the
// shared canvas and keep no frame history.
namespace {
// Origami's precession or the orbits' rate divergence, integrated so moving
// the control never jumps the motion.
float novaPhase=0;
struct NovaVertex{float x,y,z;};
NovaVertex novaRotate(NovaVertex v,float sx,float cx,float sy,float cy){
 float y=v.y*cx-v.z*sx,z=v.y*sx+v.z*cx;
 return {v.x*cy+z*sy,y,z*cy-v.x*sy};
}
RemixPoint novaProject(NovaVertex v,float W,float H){
 float perspective=2.8f/(3.3f+v.z);
 return {(W-1)*.5f+v.x*H*.42f*perspective,(H-1)*.5f+v.y*H*.42f*perspective};
}
void renderNovaPattern(uint8_t id,uint16_t W,uint16_t H,float dt,const uint8_t p[4],
 const uint16_t pal[256],PatternflowPixelWriter writer,void* context){
 if(!claimCanvas(id+300,W,H,[]{novaPhase=0;}))return;
 memset(remixPixels,0,W*H*sizeof(uint16_t));
 const float step=dt*mapControl(p[0],.08f,1.7f);
 remixClock+=step;
 const float t=remixClock,S=min(W,H)/64.f,ih=1.f/H,cx=(W-1)*.5f,cy=(H-1)*.5f;
 switch(id){
 case 0:{ // Luminous Loom: over/under satin ribbons, travelling warp and weft.
  // A smaller panel undulates less, so steep ribbons don't break into steps.
  float density=mapControl(p[1],3.f,max(4.f,min(10.f,min(W,H)/4.f))),bend=unit(p[2])*1.3f*sqrtf(S),width=mapControl(p[3],.14f,.43f);
  float rows[64],cols[64];
  for(int y=0;y<H;y++)rows[y]=fastSin(y*ih*7.f+t)*bend;
  for(int x=0;x<W;x++)cols[x]=fastSin(x*ih*8.f-t*.73f)*bend;
  const float invWidth=1.f/width;
  for(int y=0;y<H;y++)for(int x=0;x<W;x++){
   float u=x*ih*density+rows[y]+t*.17f,v=y*ih*density+cols[x]-t*.13f;
   int a=floorInt(u),b=floorInt(v);float du=fabsf(u-a-.5f)*invWidth,dv=fabsf(v-b-.5f)*invWidth;
   bool horizontal=dv<1.f&&(du>=1.f||((a+b)&1));float d=horizontal?dv:du;
   if(d<1.f){float satin=1.f-d*d,shade=.22f+.76f*satin;
    float hue=horizontal?b*.087f+.48f:a*.071f+.04f;
    // Underpasses darken, so the weave reads as depth.
    if(min(du,dv)<1.f)shade*=.8f+.2f*satin;
    remixPixels[y*W+x]=paletteColorAt(pal,hue,shade);
   }
  }break;
 }
 case 1:{ // Ferrofluid: moving implicit surface with a narrow metallic rim.
  const int count=3+p[1]*4/100;
  float blobX[7],blobY[7],strength=mapControl(p[2],.055f,.23f),metal=unit(p[3]);
  for(int i=0;i<count;i++){float a=i*(2.f*PI/count),r=.33f+.12f*smoothSin(t*.51f+i*2.f);
   blobX[i]=smoothCos(a+t*(i&1?-.41f:.32f))*r;
   blobY[i]=smoothSin(a+t*.37f)*r*.9f;}
  for(int y=0;y<H;y++)for(int x=0;x<W;x++){
   float X=(x-cx)*2.f*ih,Y=(y-cy)*2.f*ih,field=0,gx=0,gy=0,weights[7];
   for(int i=0;i<count;i++){float dx=X-blobX[i],dy=Y-blobY[i];
    float inverse=fastReciprocal(dx*dx+dy*dy+.026f),f=strength*inverse;
    field+=f;weights[i]=f*inverse;}
   if(field<1.45f)continue;
   // Outside the silhouette no normal or reflection is needed.
   for(int i=0;i<count;i++){gx+=(X-blobX[i])*weights[i];gy+=(Y-blobY[i])*weights[i];}
   float surface=clamp01((field-1.45f)*4.f),rim=clamp01(1.f-(field-1.45f)*.65f);
   float normal=(gx-gy*.8f)*fastReciprocal(1.f+fabsf(gx)+fabsf(gy));
   float reflected=.5f+.5f*fastSin(normal*9.f+field*.14f-t*.32f);
   float shine=reflected*reflected;shine*=shine;
   float light=surface*(.17f+rim*.4f+metal*shine*.42f);
   remixPixels[y*W+x]=paletteColorAt(pal,.55f+normal*.29f+rim*.15f,light);
  }break;
 }
 case 2:{ // Accretion: tilted orbital filaments, black core, lensed back rim.
  const float tilt=mapControl(p[1],.12f,.7f),core=mapControl(p[2],.08f,.23f)*H,storm=unit(p[3]);
  for(int i=0;i<45;i++){int x=hashUnit(i+41)*W,y=hashUnit(i+817)*H;
   remixInk(x,y,pal[185],.15f+.18f*fastSin(t*.35f+i));}
  // Far half first. The central silhouette occludes it; near half follows.
  for(int side=0;side<2;side++){
   if(side==1)for(int y=0;y<H;y++)for(int x=0;x<W;x++){
    float dx=x-cx,dy=y-cy;if(dx*dx+dy*dy<core*core)remixPixels[y*W+x]=0;}
   for(int ring=0;ring<7;ring++){
    float radius=core+(.04f+ring*.027f)*H;
    uint16_t color=pal[phaseByte(ring*20.f+8.f)];
    for(int j=0;j<90;j++){
     float a=j*(PI/90.f)+(side?0:PI),ca=fastCos(a),sa=fastSin(a);
     float phase=a-t*(2.8f-ring*.24f),r=radius+fastSin(phase*3.f+ring)*storm*H*.012f;
     float x=cx+ca*r,y=cy+sa*r*tilt;
     if(!side)y-=fastSin(a-PI)*H*.22f*(1.f-ring/8.f);
     float streak=.5f+.5f*fastSin(phase*4.f+ring*.8f);
     float power=(.34f+.6f*streak*streak)*(ca*.22f+.78f);
     remixPoint(x,y,color,power*1.7f);
    }
   }
  }
  // A thin photon ring around the core.
  for(int j=0;j<100;j++){float a=j*(2.f*PI/100.f);remixPoint(cx+smoothCos(a)*core,cy+smoothSin(a)*core,pal[32],.7f);}
  break;
 }
 case 3:{ // Origami: individual hinged planes in a tumbling perspective grid.
  const int grid=3+p[1]*3/100;
  const float fold=mapControl(p[2],.1f,1.65f),turn=unit(p[3]);
  const float sx=smoothSin(.48f+turn*smoothSin(t*.21f)),rx=smoothCos(.48f+turn*smoothSin(t*.21f));
  novaPhase+=step*.22f*turn;
  const float sy=smoothSin(novaPhase),ry=smoothCos(novaPhase);
  struct Face{RemixPoint v[4];float z;uint16_t color;};Face faces[36];int n=0;
  for(int y=0;y<grid;y++)for(int x=0;x<grid;x++){
   float X=(x-(grid-1)*.5f)*2.2f/grid,Y=(y-(grid-1)*.5f)*2.2f/grid;
   float angle=fold*smoothSin(t*1.1f+x*.8f+y*.57f),sn=smoothSin(angle),cs=smoothCos(angle);
   Face&f=faces[n++];f.z=0;float size=.94f/grid;
   for(int corner=0;corner<4;corner++){
    float u=(corner==0||corner==3)?-size:size,v=corner<2?-size:size;
    NovaVertex point=novaRotate({X+u*cs,Y+v,u*sn},sx,rx,sy,ry);
    f.z+=point.z;f.v[corner]=novaProject(point,W,H);
   }
   f.color=paletteColorAt(pal,(x+y*2)*.113f,.35f+.62f*fabsf(cs*rx));
  }
  // Stable back-to-front ordering prevents incorrectly overlapping facets.
  for(int i=1;i<n;i++){Face f=faces[i];int j=i;while(j&&faces[j-1].z<f.z){faces[j]=faces[j-1];j--;}faces[j]=f;}
  for(int i=0;i<n;i++){remixQuad(faces[i].v,faces[i].color);
   remixLine(faces[i].v[0],faces[i].v[1],pal[35],.55f,max(.5f,S*.65f));}
  break;
 }
 case 4:{ // Signal Orchard: discrete branching circuitry with travelling packets.
  const int trees=2+p[1]*5/100,depth=3+p[2]*3/100;
  const float surge=unit(p[3]);
  // Fixed branches carry travelling pulses.
  for(int tree=0;tree<trees;tree++){
   struct Branch{float x,y;int level;uint32_t seed;};Branch nodes[127];int used=1;
   nodes[0]={W*(tree+.5f)/trees,H-2.f,0,uint32_t(tree+913)};
   for(int q=0;q<used;q++){
    const Branch b=nodes[q];if(b.level>=depth)continue;
    float length=H*(.20f-b.level*.023f),phase=t*.6f-tree*.14f-b.level*.19f;
    float wave=fract(phase),growth=clamp01((.5f+.5f*smoothSin(t*.5f-tree*.72f))*(depth+1)-b.level);
    for(int side=0;side<2;side++){
     uint32_t seed=b.seed*1664525U+1013904223U+side*131U;
     float dx=(side?1.f:-1.f)*length*(.25f+hashUnit(seed)*.6f);
     RemixPoint start={b.x,b.y},elbow={b.x,b.y-length*.46f},end={b.x+dx,b.y-length};
     uint16_t color=pal[phaseByte(tree*33.f+b.level*25.f)];
     float light=(.12f+.2f*growth)*growth;
     if(growth>0){
      float first=min(1.f,growth/.46f),second=clamp01((growth-.46f)/.54f);
      remixLine(start,{start.x+(elbow.x-start.x)*first,start.y+(elbow.y-start.y)*first},color,light);
      if(second>0)remixLine(elbow,{elbow.x+(end.x-elbow.x)*second,elbow.y+(end.y-elbow.y)*second},color,light);
     }
     // The impulse travels along both segments, leaving a short analytic tail.
     for(int tail=0;tail<4;tail++){
      float f=wave-tail*.027f;if(f<0||f>growth)continue;
      RemixPoint a=f<.46f?start:elbow,z=f<.46f?elbow:end;
      float u=f<.46f?f/.46f:(f-.46f)/.54f;
      remixPoint(a.x+(z.x-a.x)*u,a.y+(z.y-a.y)*u,color,(1-tail*.23f)*growth*(.6f+surge));
     }
     if(used<127)nodes[used++]={end.x,end.y,b.level+1,seed};
    }
   }
  }break;
 }
 case 7: // Square Orbits fills the whole square.
 case 5:{ // Resonant Orbits: differential angular velocities repeatedly form resonances.
  const int count=scaledCount(p[1],36,216,18,min(W,H)/64.f),echoes=1+p[3]*5/100;
  const float spread=mapControl(p[2],.15f,1.f),radius=H*.43f;
  // Particle i turns at 1+i·spread.
  novaPhase+=step*spread;
  for(int i=0;i<count;i++){
   float k=(i+.5f)/count,r=radius*fastSqrt(k);
   uint16_t color=pal[phaseByte(k*205.f)];
   for(int e=echoes-1;e>=0;e--){
    float lag=e*.006f*unit(p[3]),a=.24f*(t-lag+i*(novaPhase-lag*spread));
    // Each particle has a partner mirrored through the centre.
    float dx=smoothCos(a),dy=smoothSin(a);
    float x=cx+dx*r,y=cy+dy*r;
    if(id==7){
     // Polar-to-square map: every orbit reaches its own square boundary,
     // corners included.
     float squareRadius=fastSqrt(k)/max(.0001f,max(fabsf(dx),fabsf(dy)));
     x=cx+dx*squareRadius*cx;y=cy+dy*squareRadius*cy;
    }
    remixPoint(x,y,color,(1.f-e/float(echoes))*.95f);
    remixPoint(2.f*cx-x,2.f*cy-y,pal[phaseByte(k*205.f+32)],(1.f-e/float(echoes))*.65f);
   }
  }break;
 }
 case 6:{ // Phase Architecture: differential rulings of a rotating, twisted wire surface.
  const int lines=16+p[1]*56/100;
  const float twist=mapControl(p[2],.12f,2.8f),precession=unit(p[3]);
  float tilt=.5f+smoothSin(t*.37f)*precession*.85f;
  float sx=smoothSin(tilt),rx=smoothCos(tilt),sy=smoothSin(t*.29f),ry=smoothCos(t*.29f);
  for(int i=0;i<lines;i++){
   float a=i*(2.f*PI/lines),phase=twist*smoothSin(t*.41f)+.38f*smoothSin(a*3.f-t*.27f);
   NovaVertex top=novaRotate({smoothCos(a)*.78f,-.7f,smoothSin(a)*.78f},sx,rx,sy,ry);
   NovaVertex bottom=novaRotate({smoothCos(a+phase)*.78f,.7f,smoothSin(a+phase)*.78f},sx,rx,sy,ry);
   float light=.35f+.6f*clamp01(.6f-(top.z+bottom.z)*.3f);
   uint16_t color=pal[phaseByte(i*190.f/lines)];
   remixLine(novaProject(top,W,H),novaProject(bottom,W,H),color,light,max(.6f,.82f*S));
   remixPoint(novaProject(top,W,H).x,novaProject(top,W,H).y,color,.9f);
  }break;
 }
 }
 flushCanvas(writer,context);
}
}
