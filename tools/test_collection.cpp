// Host checks of every collection pattern with the renderer compiled for the
// ESP32, included here to inspect private simulation state.
#include <cstdlib>
// Allocations the renderer may still make before they fail; -1 for no limit.
int allocationsLeft=-1;
void* limitedCalloc(size_t count,size_t size){
 if(!allocationsLeft)return nullptr;
 if(allocationsLeft>0)allocationsLeft--;
 return std::calloc(count,size);
}
#define calloc limitedCalloc
#include "../src/patterns/PatternflowPack.cpp"
#undef calloc
#include "../src/patterns/PatternCatalog.h"
#include "../src/patterns/Palettes.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

struct Frame{int size=64;unsigned writes=0;uint16_t seen=0,pixels[4096]{};};
void put(void*raw,int16_t x,int16_t y,uint16_t c){
 auto&f=*static_cast<Frame*>(raw);assert(x>=0&&y>=0&&x<f.size&&y<f.size);
 f.pixels[y*f.size+x]=c;f.writes++;f.seen|=c;
}
// Mean absolute difference of the RGB565 channels, 0–1.
float difference(const Frame&a,const Frame&b){
 float total=0;
 for(int i=0;i<a.size*a.size;i++)for(int shift:{0,5,11}){int mask=shift==5?63:31;
  total+=abs(int((a.pixels[i]>>shift)&mask)-int((b.pixels[i]>>shift)&mask))/float(mask);}
 return total/(a.size*a.size*3);
}

PatternflowPack engine;uint16_t pal[256];SourceColors rgb{pal,1.f,1.f,false};
// The firmware's colours under Original: the authored gradient, and source RGB as drawn.
void authoredColors(int v){
 const auto&colors=PATTERNFLOW_METADATA[v].originalColors;
 for(int i=0;i<256;i++){int a=i/64,b=(a+1)&3,k=i%64;unsigned c=0;
  for(int shift:{16,8,0}){int x=(colors[a]>>shift)&255,y=(colors[b]>>shift)&255;c|=(x+(y-x)*k/64)<<shift;}
  pal[i]=((c>>8)&0xF800)|((c>>5)&0x7E0)|((c>>3)&31);}
 rgb.original=true;
}
// A palette of one colour, which source RGB takes at its own value.
void paintTables(uint16_t color){for(auto&c:pal)c=color;rgb.original=false;}
// A one-colour white palette, so only light and shape remain.
void oneColour(){paintTables(0xFFFF);}
// Continues f for seconds of motion at fps, clearing each frame as the firmware does.
void advance(Frame&f,int v,const uint8_t*p,int fps,float seconds){
 for(int n=int(fps*seconds+.5f);n>0;n--){memset(f.pixels,0,sizeof f.pixels);engine.render(v,f.size,f.size,1.f/fps,p,pal,rgb,put,&f);}
}
Frame run(int v,const uint8_t*p,int size,int fps,float seconds){
 Frame f;f.size=size;engine.reset(v);advance(f,v,p,fps,seconds);return f;
}
// How far a control's extremes set a pattern apart, over frames sampled every
// half second for five seconds at 30 FPS, which every pattern draws alike;
// flat tells whether each of them was one colour.
float controlRange(int v,int c,int size,bool*flat=nullptr){
 static Frame film[10];uint8_t p[4];memcpy(p,PATTERNFLOW_METADATA[v].defaults,4);float total=0;
 if(flat)*flat=true;
 for(int pass=0;pass<2;pass++){
  p[c]=pass?100:0;Frame f;f.size=size;engine.reset(v);
  for(auto&frame:film){advance(f,v,p,30,.5f);if(pass)total+=difference(frame,f);else frame=f;
   if(flat)for(int i=0;i<size*size;i++)*flat&=f.pixels[i]==f.pixels[0];}
 }
 return total/10;
}

int main(){
 // The gallery lists exactly the variants with metadata; retired ones default to palette 0.
 bool listed[PATTERNFLOW_PATTERN_COUNT]{};
 for(int v:COLLECTION_PATTERN_IDS){assert(PATTERNFLOW_METADATA[v].name&&!listed[v]);listed[v]=true;}
 for(int v=0;v<PATTERNFLOW_PATTERN_COUNT;v++)assert(listed[v]==(PATTERNFLOW_METADATA[v].name!=nullptr)&&(listed[v]||!Palettes::Variants[v]));

 for(int size:{16,22,64})for(int v=0;v<PATTERNFLOW_PATTERN_COUNT;v++){
  const auto&meta=PATTERNFLOW_METADATA[v];authoredColors(v);
  for(int set=0;set<4;set++){
   uint8_t p[4];memcpy(p,meta.defaults,4);
   if(set==1)memset(p,0,4);
   if(set==2)memset(p,100,4);
   if(set==3){p[0]=0;p[1]=100;p[2]=0;p[3]=100;}
   Frame f;f.size=size;engine.reset(v);
   for(int n=0;n<12;n++)engine.render(v,size,size,n==10?.1f:1.f/60,p,pal,rgb,put,&f);
   assert(meta.name||!f.writes);
  }
 }
 puts("PASS every ID at 16, 22 and 64 pixels through control extremes and a frame spike; retired IDs draw nothing");

 // A pattern whose memory is short stays dark only until an allocation succeeds:
 // first with none at all, then with Ripples' canvas but not its trail.
 int starved=0;
 for(int v:COLLECTION_PATTERN_IDS)for(int allowed:{0,1}){
  if(allowed&&v!=48)continue;
  authoredColors(v);const uint8_t*p=PATTERNFLOW_METADATA[v].defaults;Frame f;f.size=22;engine.reset(v);
  allocationsLeft=allowed;advance(f,v,p,60,.1f);starved+=!f.writes;
  allocationsLeft=-1;f.writes=0;advance(f,v,p,60,.5f);assert(f.writes);
 }
 assert(starved>=20);
 printf("PASS %d patterns without memory recover once an allocation succeeds\n",starved);

 // Count controls keep their 64-pixel mapping and, on smaller panels, rise in even
 // steps across the whole control instead of resting on their floor.
 const int counts[][3]={{6,24,4},{5,20,3},{6,20,3},{14,90,10},{36,216,18},{8,32,4},{10,28,4},{16,44,8}};
 for(const auto&c:counts)for(float scale:{16/64.f,22/64.f,.3f,1.f}){
  int previous=-1,run=0,longest=0;const int first=scaledCount(0,c[0],c[1],c[2],scale),last=scaledCount(100,c[0],c[1],c[2],scale);
  assert(first>=c[2]&&last>first);
  for(int v=0;v<=100;v++){const int n=scaledCount(v,c[0],c[1],c[2],scale);
   if(scale==1.f)assert(n==c[0]+v*(c[1]-c[0])/100);
   assert(n>=previous);run=n==previous?run+1:1;longest=max(longest,run);previous=n;}
  assert(longest<=100/(last-first)+1);
 }
 puts("PASS count controls: the 64-pixel mapping unchanged, and even steps without dead zones at 16 and 22 pixels");

 int paletteDrawn=0;
 for(int v:COLLECTION_PATTERN_IDS){
  const auto&meta=PATTERNFLOW_METADATA[v];const uint8_t*p=meta.defaults;authoredColors(v);
  Frame base=run(v,p,64,60,5);
  assert(difference(base,run(v,p,64,30,5))<.005f&&difference(base,run(v,p,64,120,5))<.005f);
  Frame again=run(v,p,64,60,5);assert(!memcmp(again.pixels,base.pixels,sizeof base.pixels));
  advance(again,v,p,60,.5f);assert(difference(base,again)>.002f);
  constexpr float VISIBLE=.002f;  // The least change a control may make.
  for(int c=0;c<4;c++)if(meta.controlLabel[c])for(int size:{16,22,64}){
   const float range=controlRange(v,c,size);
   if(range<VISIBLE){printf("FAIL %s: %s barely changes it at %d pixels (%.4f)\n",meta.name,meta.controlLabel[c],size,range);return 1;}
  }
  // Under one colour only light and shape remain, so a control that changes them
  // less than visibly only shifts colour. A pattern drawn by palette position
  // alone goes flat there, so its controls are judged by hand.
  oneColour();bool flat=true;
  for(int c=0;c<4;c++)if(meta.controlLabel[c])for(int size:{16,22,64}){
   bool flatHere;const float range=controlRange(v,c,size,&flatHere);flat&=flatHere;
   if(!flatHere&&range<VISIBLE){printf("FAIL %s: %s only shifts colour at %d pixels (%.4f)\n",meta.name,meta.controlLabel[c],size,range);return 1;}
  }
  paletteDrawn+=flat;
  // Colour comes only from the tables, and a black table blacks out the next frame.
  paintTables(0xF800);Frame red=run(v,p,22,60,5);assert(red.seen&&!(red.seen&0x7FF));
  paintTables(0);red.seen=0;engine.render(v,22,22,1.f/60,p,pal,rgb,put,&red);assert(!red.seen);
 }
 printf("PASS %d patterns: frame-rate independent, repeatable and moving; colour from the tables; every control visibly changes light and shape at 16, 22 and 64 pixels, not only colour (%d patterns drawn by palette position alone are judged by hand)\n",COLLECTION_PATTERN_COUNT,paletteDrawn);

 // Folded Cross at its fewest folds still spreads several colours across the panel.
 authoredColors(6);{
  uint8_t p[4];memcpy(p,PATTERNFLOW_METADATA[6].defaults,4);p[2]=0;
  for(int size:{16,22,64}){Frame f;f.size=size;engine.reset(6);
   for(int n=0;n<40;n++){advance(f,6,p,60,.25f);static uint16_t count[65536];int most=0;
    for(int i=0;i<size*size;i++)most=max(most,int(++count[f.pixels[i]]));
    for(int i=0;i<size*size;i++)count[f.pixels[i]]=0;
    assert(most<size*size*9/10);}
  }
 }
 puts("PASS Folded Cross never turns the panel into one colour, even at its fewest folds");

 authoredColors(43);{
  const uint8_t*p=PATTERNFLOW_METADATA[43].defaults;Frame f=run(43,p,64,60,3.2f),start=f;
  advance(f,43,p,60,.4f);Frame end=f;advance(f,43,p,60,.4f);
  assert(difference(start,end)>.005f&&difference(end,f)>.005f);
 }
 puts("PASS Motion Graphics moves continuously, without holds");

 authoredColors(45);{
  const uint8_t*p=PATTERNFLOW_METADATA[45].defaults;Frame f=run(45,p,64,60,6);int lit=0;
  for(auto c:f.pixels)lit+=c!=sourceColor(rgb,0x171815);
  assert(lit>200&&lit<=7*(14+p[1]*76/100));
 }
 puts("PASS Bloom draws single-pixel ribbons across the square, not discs");

 // A teal body stays teal where rings overlap, though unused stops are white.
 for(int i=0;i<256;i++)pal[i]=i>=64&&i<80?uint16_t((4<<11)|(42<<5)|20):0xFFFF;
 for(int size:{16,22,64}){
  const uint8_t p[4]={80,65,100,0};Frame f;f.size=size;engine.reset(48);unsigned lit=0;
  for(int n=0;n<240;n++){
   advance(f,48,p,60,1.f/60);
   for(int i=0;i<size*size;i++){int r=f.pixels[i]>>11,g=(f.pixels[i]>>5)&63,b=f.pixels[i]&31;assert(r<=g*4/42+2&&b<=g*20/42+2);lit+=f.pixels[i]!=0;}
  }
  assert(lit>1000);
 }
 puts("PASS Ripples keep their pigment through overlap at 16, 22 and 64 pixels");

 // Raindrops and Plasma step at a fixed 30 Hz, so any frame rate draws the same frames.
 for(int v:{63,64}){
  authoredColors(v);const uint8_t*p=PATTERNFLOW_METADATA[v].defaults;
  // Hashes the frame at each of ten seconds.
  auto film=[&](int size,int fps){uint64_t hash=1469598103934665603ULL;Frame f;f.size=size;engine.reset(v);
   for(int second=0;second<10;second++){advance(f,v,p,fps,1);for(int i=0;i<size*size;i++)hash=(hash^f.pixels[i])*1099511628211ULL;}
   return hash;};
  for(int size:{16,22,64}){const uint64_t base=film(size,30);assert(base==film(size,60)&&base==film(size,120));}
 }
 puts("PASS Raindrops and Plasma draw identical frames every second at 30, 60 and 120 FPS");

 authoredColors(58);{
  const uint8_t*p=PATTERNFLOW_METADATA[58].defaults;run(58,p,64,60,20);
  uint8_t tiles[sizeof pongTiles];PongBall balls[2];memcpy(tiles,pongTiles,sizeof tiles);memcpy(balls,pongBalls,sizeof balls);
  for(int fps:{30,120,240}){run(58,p,64,fps,20);assert(!memcmp(tiles,pongTiles,sizeof tiles)&&!memcmp(balls,pongBalls,sizeof balls));}
  paintTables(0xF800);for(auto c:run(58,p,64,60,2).pixels)assert(c==0xF800);
 }
 initializePongBoard(24);assert(pongOwner(11,12)==0&&pongOwner(12,12)==1);
 pongBalls[0]={11.8f,12.5f,1.f,0.f};pongBalls[1]={20.f,20.f,-.7f,.7f};
 stepPongWars(1.5f,.45f,0);
 assert(pongOwner(12,12)==0&&pongBalls[0].vx<0);
 for(int grid:{8,24,32})for(float radius:{.25f,.475f,.7f}){
  initializePongBoard(grid);unsigned changes=0;
  for(int i=0;i<240*180;i++){
   uint32_t before=pongSeed;stepPongWars(1.5f,radius,1.f);changes+=before!=pongSeed;
   for(const auto&b:pongBalls){assert(isfinite(b.x)&&isfinite(b.y));assert(b.x>=radius&&b.x<=grid-radius&&b.y>=radius&&b.y<=grid-radius);}
  }
  assert(changes>100);
 }
 puts("PASS Pong Wars: identical physics at 30–240 FPS, every pixel painted, tile conversion, and 27 bounded minutes at extremes");

 for(int v=65;v<68;v++){
  authoredColors(v);const uint8_t*p=PATTERNFLOW_METADATA[v].defaults;
  for(int size:{16,22,64}){Frame f=run(v,p,size,60,3);assert(f.writes==unsigned(180*size*size)&&!memcmp(f.pixels,run(v,p,size,60,3).pixels,sizeof f.pixels));}
 }
 // Cycling visits every other pattern in gallery order and steps over the flashing ones.
 int visited=0;
 for(int index=nextCycledIndex(-1);index!=0||!visited;index=nextCycledIndex(index)){
  assert(index<CORE_PATTERN_COUNT||!isFlashingVariant(COLLECTION_PATTERN_IDS[index-CORE_PATTERN_COUNT]));
  if(++visited>GALLERY_PATTERN_COUNT)break;
 }
 assert(visited==GALLERY_PATTERN_COUNT-3);
 for(int v=65;v<68;v++)assert(isFlashingVariant(v));
 assert(nextCycledIndex(GALLERY_PATTERN_COUNT-1)==0);  // From a flashing pattern picked by hand.
 puts("PASS flashing patterns write every pixel of every frame, repeat after a reset, and cycling skips them");
 engine.reset(0);
}
