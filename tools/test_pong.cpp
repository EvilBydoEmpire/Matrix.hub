// Include renderer in this TU to inspect private simulation state, not expose
// test/debug endpoints in production firmware.
#include "../src/patterns/PatternflowPack.cpp"
#include <cassert>
#include <cstdio>
struct Frame{uint16_t pixels[4096]{};};
void pixel(void*raw,int16_t x,int16_t y,uint16_t c){assert(x>=0&&y>=0&&x<64&&y<64);static_cast<Frame*>(raw)->pixels[y*64+x]=c;}
struct Snapshot{uint8_t board[128];PongBall balls[2];Frame frame;};
int main(){
 PatternflowPack renderer;uint16_t pal[256],rgb[4096]{};
 for(int i=0;i<256;i++)pal[i]=i<128?0xd75c:0x1946;
 auto run=[&](int fps,const uint8_t*p,float duration){renderer.reset(58);Frame frame;
  for(int i=0;i<int(fps*duration);i++)renderer.render(58,64,64,1.f/fps,p,pal,rgb,pixel,&frame);
  Snapshot result;memcpy(result.board,pongTiles,128);memcpy(result.balls,pongBalls,sizeof(pongBalls));result.frame=frame;return result;};
 const auto p=PATTERNFLOW_METADATA[58].defaults;
 auto base=run(60,p,20.f);
 for(int fps:{30,120,240}){auto other=run(fps,p,20.f);
  assert(memcmp(other.board,base.board,128)==0);assert(memcmp(other.balls,base.balls,sizeof(pongBalls))==0);}
 puts("PASS identical 20-second physics at 30/60/120/240 FPS");
 for(int control=0;control<4;control++){uint8_t changed[4];memcpy(changed,p,4);changed[control]=0;auto other=run(60,changed,20.f);
  assert(memcmp(other.frame.pixels,base.frame.pixels,sizeof(Frame))!=0);}
 puts("PASS all four algorithm controls change the result");
 initializePongBoard(24);assert(pongOwner(11,12)==0&&pongOwner(12,12)==1);
 pongBalls[0]={11.8f,12.5f,1.f,0.f};pongBalls[1]={20.f,20.f,-.7f,.7f};
 stepPongWars(1.5f,.45f,0);
 assert(pongOwner(12,12)==0&&pongBalls[0].vx<0);
 puts("PASS enemy tile converts and the ball reflects");
 for(int grid:{8,24,32})for(float radius:{.25f,.475f,.7f}){
  initializePongBoard(grid);unsigned changes=0;
  for(int i=0;i<240*180;i++){
   uint32_t before=pongSeed;stepPongWars(1.5f,radius,1.f);changes+=before!=pongSeed;
   for(const auto&b:pongBalls){assert(isfinite(b.x)&&isfinite(b.y));assert(b.x>=radius&&b.x<=grid-radius&&b.y>=radius&&b.y<=grid-radius);}
  }
  assert(changes>100);
 }
 puts("PASS 27 simulated minutes at extreme speed: bounded, finite, no stalled balls");
 memset(pal,0,sizeof(pal));auto black=run(60,p,2.f);for(auto c:black.frame.pixels)assert(c==0);
 for(auto&c:pal)c=0xf800;auto red=run(60,p,2.f);for(auto c:red.frame.pixels)assert(c==0xf800);
 puts("PASS palette recoloring and immediate zero-brightness rendering");
 renderer.reset(0);
}
