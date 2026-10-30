// Three optical studies with deliberate aliasing: the Aliasing control moves
// each from a soft checker to a razor checker and then pixel-parity interference.
namespace {
void renderAcidPattern(int index,uint16_t W,uint16_t H,float dt,const uint8_t p[4],
 const uint16_t* pal,PatternflowPixelWriter write,void* context){
 if(remixIndex!=400+index){resetRemixPattern();remixIndex=400+index;}
 remixClock+=dt*mapControl(p[0],.08f,2.4f);
 const float t=remixClock,inv=2.f/min(W,H),cx=(W-1)*.5f,cy=(H-1)*.5f;
 // Grid density runs from a coarse checker to the finest grid the panel still
 // resolves: cells about a pixel wide at 64 pixels and two at 16.
 const float S=min(W,H)/64.f,root=sqrtf(S);
 const float density=mapControl(p[1],3.f*root,28.f*S*root),warp=unit(p[2]),alias=unit(p[3]);
 // Acid Cathedral's angular tiles: sixteen at 32 pixels and up, eight at 16.
 const float tiles=2.f*constrain((int)lroundf(16.f*S),4,8);
 const float zoom=1.15f+.65f*smoothSin(t*.57f);
 const float ca=smoothCos(t*.17f),sa=smoothSin(t*.17f);
 float rows[64],cols[64];
 for(int y=0;y<H;y++)rows[y]=smoothSin((y-cy)*inv*3.2f+t*.83f);
 for(int x=0;x<W;x++)cols[x]=smoothCos((x-cx)*inv*3.7f-t*.67f);
 for(int y=0;y<H;y++)for(int x=0;x<W;x++){
  const float px=(x-cx)*inv,py=(y-cy)*inv;
  float u,v,hue,light;
  if(index==0){
   // Square-perspective throat, counter-rotating arches and radial zoom.
   const float qx=px*ca-py*sa,qy=px*sa+py*ca;
   const float radius=max(fabsf(qx),fabsf(qy));
   const float lens=1.f/(.22f+radius*(.8f+.3f*smoothSin(t*.31f)));
   const float arch=warp*1.8f*smoothSin(radius*8.f-t*1.3f);
   // Angular tiles meet square depth bands: a readable throat even when the
   // innermost bands cross the pixel sampling limit.
   u=polarAngle[y*W+x]*(tiles*.5f/PI)+t*.43f+arch*1.5f;
   v=lens*zoom*density*.48f-t*2.f+arch*.45f;
   // Colour runs out from the palette's body in the throat to its accent at the rim.
   hue=.55f*radius-.04f+arch*.04f+t*.015f;
  }else if(index==1){
   // Two sliding lattices cross the sampling limit: coherent moire sheets.
   const float bend=warp*(rows[y]*cols[x]*2.f+.6f*smoothSin((px+py)*4.f-t));
   const float z=density*zoom;
   u=(px+py*.34f+bend*.23f)*z+t*.7f;
   v=(py-px*.31f-bend*.29f)*z-t*.53f;
   const float phase=warp*smoothSin(px*2.5f-py*3.f+t*.8f)*6.f;
   u+=phase;v-=phase*.73f;
   hue=.56f+.14f*smoothSin((px-py)*2.f+t*.2f)+bend*.06f;
  }else{
   // A liquid saddle folds grid lines into circuity islands and vortex seams.
   const float w1=rows[y]+warp*smoothSin(cols[x]*2.f+py*4.f-t*.9f);
   const float w2=cols[x]+warp*smoothCos(rows[y]*2.f+px*4.f+t*.7f);
   const float saddle=px*py*warp;
   u=(px*zoom+warp*w1*.62f+saddle)*density;
   v=(py*zoom+warp*w2*.62f-saddle)*density;
   hue=.68f+.1f*w1-.06f*w2+t*.012f;
  }
  const int iu=floorInt(u),iv=floorInt(v);
  const float fu=u-iu,fv=v-iv;
  const float edge=min(min(fu,1.f-fu),min(fv,1.f-fv));
  const float softness=.23f*(1.f-alias)+.015f;
  const float contrast=clamp01(edge/softness);
  float sign=((iu+iv)&1)?1.f:-1.f;
  if(alias>.55f){
   // Pixel parity weighs more on a panel's larger pixels, so it blends in
   // less there and the grid stays legible.
   const float wave=smoothSin((u-v)*.37f+t),parity=(alias-.55f)*1.6f*root;
   const float micro=((x+y+floorInt((u+v)*.23f))&1)?1.f:-1.f;
   sign=sign*(1.f-parity)+(micro*wave)*parity;
  }
  light=clamp01(.5f+.5f*sign*contrast);
  light*=light; // deep black ink, rich palette highlights, no white wash
  write(context,x,y,paletteColorAt(pal,hue,light));
 }
}
}
