// Pong Wars: native adaptation of Koen van Gilst's MIT-licensed reference.
// Source/notice: patterns/imports/pong-wars/LICENSE and THIRD_PARTY_NOTICES.md.
// Same two opposing balls / territory conversion; new fixed-step collision
// handling and small bit-packed grid suited to 16/22/64-pixel ESP32 panels.
namespace {
struct PongBall{float x,y,vx,vy;};
uint8_t pongTiles[128]{}; // At most 32*32 cells; 1 bit per territory.
PongBall pongBalls[2]{};
int pongGrid=0;
float pongCarry=0;
uint32_t pongSeed=0x607D123BU;
void resetPongWars(){pongGrid=0;pongCarry=0;pongSeed=0x607D123BU;}
inline int pongOwner(int x,int y){int n=y*pongGrid+x;return (pongTiles[n>>3]>>(n&7))&1;}
inline void pongSetOwner(int x,int y,int owner){int n=y*pongGrid+x;uint8_t mask=uint8_t(1U<<(n&7));
 if(owner)pongTiles[n>>3]|=mask;else pongTiles[n>>3]&=uint8_t(~mask);}
void initializePongBoard(int grid){
 resetPongWars();pongGrid=grid;memset(pongTiles,0,sizeof(pongTiles));
 for(int y=0;y<grid;y++)for(int x=grid/2;x<grid;x++)pongSetOwner(x,y,1);
 pongBalls[0]={grid*.25f,grid*.5f,.70710678f,-.70710678f};
 pongBalls[1]={grid*.75f,grid*.5f,-.70710678f,.70710678f};
}
bool pongClaim(float x,float y,float radius,int team){
 bool hit=false;
 for(int row=max(0,floorInt(y-radius));row<=min(pongGrid-1,floorInt(y+radius));row++)
  for(int col=max(0,floorInt(x-radius));col<=min(pongGrid-1,floorInt(x+radius));col++){
   if(pongOwner(col,row)==team)continue;
   float dx=x-constrain(x,float(col),float(col+1)),dy=y-constrain(y,float(row),float(row+1));
   if(dx*dx+dy*dy<radius*radius){pongSetOwner(col,row,team);hit=true;}
  }
 return hit;
}
void stepPongWars(float speed,float radius,float chaos){
 // At the maximum controls, displacement is <= .2 cells per 240 Hz tick.
 // Axis-separated circle/AABB tests cannot tunnel through a complete cell,
 // and reflect each velocity component once even when several cells are hit.
 float distance=speed*pongGrid*(1.f/240.f);
 for(int team=0;team<2;team++){
  auto&b=pongBalls[team];bool hit=false;
  b.x=constrain(b.x,radius,pongGrid-radius);b.y=constrain(b.y,radius,pongGrid-radius);
  float nx=b.x+b.vx*distance;
  bool hitX=nx<radius||nx>pongGrid-radius;nx=constrain(nx,radius,pongGrid-radius);
  if(pongClaim(nx,b.y,radius,team))hitX=true;else b.x=nx;
  if(hitX){b.vx=-b.vx;hit=true;}
  float ny=b.y+b.vy*distance;
  bool hitY=ny<radius||ny>pongGrid-radius;ny=constrain(ny,radius,pongGrid-radius);
  if(pongClaim(b.x,ny,radius,team))hitY=true;else b.y=ny;
  if(hitY){b.vy=-b.vy;hit=true;}
  if(hit&&chaos>0){
   b.vx+=(randomUnit(pongSeed)-.5f)*chaos*.55f;
   b.vy+=(randomUnit(pongSeed)-.5f)*chaos*.55f;
   if(fabsf(b.vx)<.2f)b.vx=b.vx<0?-.2f:.2f;
   if(fabsf(b.vy)<.2f)b.vy=b.vy<0?-.2f:.2f;
   float inverse=1.f/sqrtf(b.vx*b.vx+b.vy*b.vy);b.vx*=inverse;b.vy*=inverse;
  }
 }
}
void renderPongWars(uint16_t W,uint16_t H,float dt,const uint8_t p[4],
 const uint16_t pal[256],PatternflowPixelWriter writer,void*context){
 if(remixIndex!=400||remixW!=W||remixH!=H){
  resetRemixPattern();resetPongWars();remixW=W;remixH=H;remixIndex=400;
  remixPixels=(uint16_t*)calloc(W*H,sizeof(uint16_t));
 }
 if(!remixPixels)return;
 int grid=8+(100-p[1])*24/100;
 if(grid!=pongGrid)initializePongBoard(grid);
 float radius=mapControl(p[2],.25f,.7f),speed=mapControl(p[0],.1f,1.5f),chaos=unit(p[3]);
 pongCarry+=constrain(dt,0.f,.1f)*240.f;
 int ticks=min(24,int(pongCarry+1e-5f));pongCarry=max(0.f,pongCarry-ticks);
 for(int i=0;i<ticks;i++)stepPongWars(speed,radius,chaos);
 const uint16_t territory[2]={pal[0],pal[128]};
 int columns[64];for(int x=0;x<W;x++)columns[x]=x*grid/W;
 for(int y=0;y<H;y++){int row=y*grid/H;for(int x=0;x<W;x++)remixPixels[y*W+x]=territory[pongOwner(columns[x],row)];}
 for(int team=0;team<2;team++){
  // Opposite-color balls on each home territory, as in the reference. Source
  // over (not additive/lighten) is essential for the DARK ball to be visible.
  float x=pongBalls[team].x*W/grid-.5f,y=pongBalls[team].y*H/grid-.5f,r=radius*min(W,H)/grid;
  uint16_t color=pal[team?64:192];
  for(int py=max(0,floorInt(y-r-.5f));py<=min(int(H)-1,int(ceilf(y+r+.5f)));py++)
   for(int px=max(0,floorInt(x-r-.5f));px<=min(int(W)-1,int(ceilf(x+r+.5f)));px++)
    whitneyOver(px,py,color,clamp01(r+.5f-fastHypotApprox(px-x,py-y)));
 }
 for(int y=0;y<H;y++)for(int x=0;x<W;x++)writer(context,x,y,remixPixels[y*W+x]);
}
}
