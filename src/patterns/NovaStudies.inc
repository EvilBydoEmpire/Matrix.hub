// Seven original native-64 studies. The last two are inspired by differential
// motion / visual counterpoint in John Whitney's work, not copied source code.
// One shared RGB565 canvas; no simulation textures, frame history or dithering.
namespace {
struct NovaVertex{float x,y,z;};
inline float novaPositiveReciprocal(float x){
 // Positive, bounded inputs only. Two Newton iterations already exceed the
 // panel's color precision; avoid repeated abs/sign tests in the fluid field.
 union{float f;uint32_t bits;} v={x};v.bits=0x7EF311C3U-v.bits;
 float r=v.f;r*=2.f-x*r;r*=2.f-x*r;return r;
}
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
 if(remixIndex!=int(id)+300||remixW!=W||remixH!=H){
  resetRemixPattern();remixW=W;remixH=H;remixIndex=id+300;
  remixPixels=(uint16_t*)calloc(W*H,sizeof(uint16_t));
 }
 if(!remixPixels)return;
 memset(remixPixels,0,W*H*sizeof(uint16_t));
 remixClock+=constrain(dt,0.f,.1f)*mapControl(p[0],.08f,1.7f);
 const float t=remixClock,S=min(W,H)/64.f,ih=1.f/H,cx=(W-1)*.5f,cy=(H-1)*.5f;
 switch(id){
 case 0:{ // Luminous Loom: over/under satin ribbons, traveling warp and weft.
  float density=mapControl(p[1],3.f,10.f),bend=unit(p[2])*1.3f,width=mapControl(p[3],.14f,.43f);
  float rows[64],cols[64];
  for(int y=0;y<H;y++)rows[y]=fastSinApprox(y*ih*7.f+t)*bend;
  for(int x=0;x<W;x++)cols[x]=fastSinApprox(x*ih*8.f-t*.73f)*bend;
  const float invWidth=1.f/width;
  for(int y=0;y<H;y++)for(int x=0;x<W;x++){
   float u=x*ih*density+rows[y]+t*.17f,v=y*ih*density+cols[x]-t*.13f;
   int a=floorInt(u),b=floorInt(v);float du=fabsf(u-a-.5f)*invWidth,dv=fabsf(v-b-.5f)*invWidth;
   bool horizontal=dv<1.f&&(du>=1.f||((a+b)&1));float d=horizontal?dv:du;
   if(d<1.f){float satin=1.f-d*d,shade=.22f+.76f*satin;
    float hue=horizontal?b*.087f+.48f:a*.071f+.04f;
    // Dark underpasses read as depth, without adding expensive shadow passes.
    if(min(du,dv)<1.f)shade*=.8f+.2f*satin;
    remixPixels[y*W+x]=colorAt(pal,hue,shade);
   }
  }break;
 }
 case 1:{ // Ferrofluid: moving implicit surface with a narrow metallic rim.
  const int count=3+p[1]*4/100;
  float blobX[7],blobY[7],strength=mapControl(p[2],.055f,.23f),metal=unit(p[3]);
  for(int i=0;i<count;i++){float a=i*(2.f*PI/count),r=.33f+.12f*silkySin(t*.51f+i*2.f);
   blobX[i]=silkyCos(a+t*(i&1?-.41f:.32f))*r;
   blobY[i]=silkySin(a+t*.37f)*r*.9f;}
  for(int y=0;y<H;y++)for(int x=0;x<W;x++){
   float X=(x-cx)*2.f*ih,Y=(y-cy)*2.f*ih,field=0,gx=0,gy=0,weights[7];
   for(int i=0;i<count;i++){float dx=X-blobX[i],dy=Y-blobY[i];
    float inverse=novaPositiveReciprocal(dx*dx+dy*dy+.026f),f=strength*inverse;
    field+=f;weights[i]=f*inverse;}
   if(field<1.45f)continue;
   // Outside the silhouette no normal or reflection is needed. The original
   // implementation paid for both gradients at every background pixel too.
   for(int i=0;i<count;i++){gx+=(X-blobX[i])*weights[i];gy+=(Y-blobY[i])*weights[i];}
   float surface=clamp01((field-1.45f)*4.f),rim=clamp01(1.f-(field-1.45f)*.65f);
   float normal=(gx-gy*.8f)*novaPositiveReciprocal(1.f+fabsf(gx)+fabsf(gy));
   float reflected=.5f+.5f*fastSinApprox(normal*9.f+field*.14f-t*.32f);
   float shine=reflected*reflected;shine*=shine;
   float light=surface*(.17f+rim*.4f+metal*shine*.42f);
   remixPixels[y*W+x]=colorAt(pal,.55f+normal*.29f+rim*.15f,light);
  }break;
 }
 case 2:{ // Accretion: tilted orbital filaments, black core, lensed back rim.
  const float tilt=mapControl(p[1],.12f,.7f),core=mapControl(p[2],.08f,.23f)*H,storm=unit(p[3]);
  for(int i=0;i<45;i++){int x=ihash(i+41)*W,y=ihash(i+817)*H;
   remixInk(x,y,pal[185],.15f+.18f*fastSinApprox(t*.35f+i));}
  // Far half first. The central silhouette occludes it; near half follows.
  for(int side=0;side<2;side++){
   if(side==1)for(int y=0;y<H;y++)for(int x=0;x<W;x++){
    float dx=x-cx,dy=y-cy;if(dx*dx+dy*dy<core*core)remixPixels[y*W+x]=0;}
   for(int ring=0;ring<7;ring++){
    float radius=core+(.04f+ring*.027f)*H;
    uint16_t color=pal[phaseByte(ring*20.f+8.f)];
    for(int j=0;j<90;j++){
     float a=j*(PI/90.f)+(side?0:PI),ca=fastCosApprox(a),sa=fastSinApprox(a);
     float phase=a-t*(2.8f-ring*.24f),r=radius+fastSinApprox(phase*3.f+ring)*storm*H*.012f;
     float x=cx+ca*r,y=cy+sa*r*tilt;
     if(!side)y-=fastSinApprox(a-PI)*H*.22f*(1.f-ring/8.f);
     float streak=.5f+.5f*fastSinApprox(phase*4.f+ring*.8f);
     float power=(.34f+.6f*streak*streak)*(ca*.22f+.78f);
     remixPoint(x,y,color,power*1.7f);
    }
   }
  }
  // Thin photon ring: analytic coverage, not a broad blurry disc.
  for(int j=0;j<100;j++){float a=j*(2.f*PI/100.f);remixPoint(cx+silkyCos(a)*core,cy+silkySin(a)*core,pal[32],.7f);}
  break;
 }
 case 3:{ // Origami: individual hinged planes in a tumbling perspective grid.
  const int grid=3+p[1]*3/100;
  const float fold=mapControl(p[2],.1f,1.65f),turn=unit(p[3]);
  const float sx=silkySin(.48f+turn*silkySin(t*.21f)),rx=silkyCos(.48f+turn*silkySin(t*.21f));
  const float sy=silkySin(t*.22f*turn),ry=silkyCos(t*.22f*turn);
  struct Face{RemixPoint v[4];float z;uint16_t color;};Face faces[36];int n=0;
  for(int y=0;y<grid;y++)for(int x=0;x<grid;x++){
   float X=(x-(grid-1)*.5f)*2.2f/grid,Y=(y-(grid-1)*.5f)*2.2f/grid;
   float angle=fold*silkySin(t*1.1f+x*.8f+y*.57f),sn=silkySin(angle),cs=silkyCos(angle);
   Face&f=faces[n++];f.z=0;float size=.94f/grid;
   for(int corner=0;corner<4;corner++){
    float u=(corner==0||corner==3)?-size:size,v=corner<2?-size:size;
    NovaVertex point=novaRotate({X+u*cs,Y+v,u*sn},sx,rx,sy,ry);
    f.z+=point.z;f.v[corner]=novaProject(point,W,H);
   }
   f.color=colorAt(pal,(x+y*2)*.113f,.35f+.62f*fabsf(cs*rx));
  }
  // Stable back-to-front ordering prevents incorrectly overlapping facets.
  for(int i=1;i<n;i++){Face f=faces[i];int j=i;while(j&&faces[j-1].z<f.z){faces[j]=faces[j-1];j--;}faces[j]=f;}
  for(int i=0;i<n;i++){remixQuad(faces[i].v,faces[i].color);
   remixLine(faces[i].v[0],faces[i].v[1],pal[35],.55f,max(.5f,S*.65f));}
  break;
 }
 case 4:{ // Signal Orchard: discrete branching circuitry with traveling packets.
  const int trees=2+p[1]*5/100,depth=3+p[2]*3/100;
  const float surge=unit(p[3]);
  // Deterministic geometry, continuous traveling energy; no random flicker.
  for(int tree=0;tree<trees;tree++){
   struct Branch{float x,y;int level;uint32_t seed;};Branch nodes[127];int used=1;
   nodes[0]={W*(tree+.5f)/trees,H-2.f,0,uint32_t(tree+913)};
   for(int q=0;q<used;q++){
    const Branch b=nodes[q];if(b.level>=depth)continue;
    float length=H*(.20f-b.level*.023f),phase=t*.6f-tree*.14f-b.level*.19f;
    float wave=fract(phase),growth=clamp01((.5f+.5f*silkySin(t*.5f-tree*.72f))*(depth+1)-b.level);
    for(int side=0;side<2;side++){
     uint32_t seed=b.seed*1664525U+1013904223U+side*131U;
     float dx=(side?1.f:-1.f)*length*(.25f+ihash(seed)*.6f);
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
 case 7: // Full-square v2; original case 5 remains unchanged.
 case 5:{ // Whitney: differential angular velocities repeatedly form resonances.
  const int count=36+p[1]*180/100,echoes=1+p[3]*5/100;
  const float spread=mapControl(p[2],.15f,1.f),radius=H*.43f;
  for(int i=0;i<count;i++){
   float k=(i+.5f)/count,r=radius*fastSqrtApprox(k),rate=1.f+i*spread;
   uint16_t color=pal[phaseByte(k*205.f)];
   for(int e=echoes-1;e>=0;e--){
    float clock=t-e*.006f*unit(p[3]),a=clock*.24f*rate;
    // Counter-rotating partners form a pointillist canon, not shaded spheres.
    float dx=silkyCos(a),dy=silkySin(a);
    float x=cx+dx*r,y=cy+dy*r;
    if(id==7){
     // Polar-to-square map: every orbit reaches its corresponding square
     // boundary, including corners. No crop/zoom and no extra framebuffer.
     float squareRadius=fastSqrtApprox(k)/max(.0001f,max(fabsf(dx),fabsf(dy)));
     x=cx+dx*squareRadius*cx;y=cy+dy*squareRadius*cy;
    }
    remixPoint(x,y,color,(1.f-e/float(echoes))*.95f);
    remixPoint(2.f*cx-x,2.f*cy-y,pal[phaseByte(k*205.f+32)],(1.f-e/float(echoes))*.65f);
   }
  }break;
 }
 case 6:{ // Whitney: differential rulings of a rotating, twisted wire surface.
  const int lines=16+p[1]*56/100;
  const float twist=mapControl(p[2],.12f,2.8f),precession=unit(p[3]);
  float tilt=.5f+silkySin(t*.37f)*precession*.85f;
  float sx=silkySin(tilt),rx=silkyCos(tilt),sy=silkySin(t*.29f),ry=silkyCos(t*.29f);
  for(int i=0;i<lines;i++){
   float a=i*(2.f*PI/lines),phase=twist*silkySin(t*.41f)+.38f*silkySin(a*3.f-t*.27f);
   NovaVertex top=novaRotate({silkyCos(a)*.78f,-.7f,silkySin(a)*.78f},sx,rx,sy,ry);
   NovaVertex bottom=novaRotate({silkyCos(a+phase)*.78f,.7f,silkySin(a+phase)*.78f},sx,rx,sy,ry);
   float light=.35f+.6f*clamp01(.6f-(top.z+bottom.z)*.3f);
   uint16_t color=pal[phaseByte(i*190.f/lines)];
   remixLine(novaProject(top,W,H),novaProject(bottom,W,H),color,light,max(.6f,.82f*S));
   remixPoint(novaProject(top,W,H).x,novaProject(top,W,H).y,color,.9f);
  }break;
 }
 }
 for(int y=0;y<H;y++)for(int x=0;x<W;x++)writer(context,x,y,remixPixels[y*W+x]);
}
}
