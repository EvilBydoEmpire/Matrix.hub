// Renderers 0–16 here adapt Patternflow works and are CC-BY-SA-4.0, except
// Death Star, which adapts PsiOp under the MIT licence (patterns/imports/psiop).
// docs/THIRD_PARTY_NOTICES.md credits every adaptation.

// ESP32 has float multiply/add hardware but division is a software routine.
// Permit reciprocal factoring and inline the shader helpers; retain NaN/Inf
// checks (do not enable finite-math-only / the blanket -ffast-math option).
// Ahead of every include, because GCC inlines only functions compiled with the
// same options: std::min and std::max would otherwise stay calls.
#pragma GCC optimize ("O3", "no-math-errno", "unsafe-math-optimizations")

#include "PatternflowPack.h"

#include <Arduino.h>
#include <math.h>

// Arduino's PI macro is a double. On Xtensa that silently promotes shader
// expressions to software-emulated double arithmetic; keep this renderer float.
#undef PI
#define PI 3.14159265358979323846f

#include "FastMath.h"
#include "Rgb565.h"

const PatternflowMetadata PATTERNFLOW_METADATA[PATTERNFLOW_PATTERN_COUNT] = {
  {"Vector Field Flow", {"Turbulence", "Velocity", "Density", nullptr}, {50,35,50,10}, {0x174d62,0x3dadae,0xc4e3d4,0x3dadae}},
  {"Chromatic Vortex", {"Split", "Swirl speed", "Density", nullptr}, {40,25,40,50}, {0x283b8c,0x4acac8,0xd3b2de,0x4acac8}},
  {"Liquid Ripple", {"Refraction", "Speed", "Waves", "Specular"}, {40,40,50,20}, {0x103b59,0x459eb2,0xcae1d6,0x459eb2}},
  {"Kaleidoscope", {"Symmetry", "Speed", "Core depth", "Twist"}, {30,30,50,40}, {0x622e75,0xc88098,0xecd5aa,0xc88098}},
  {"Isometric Pillars", {"Grid density", "Speed", nullptr, nullptr}, {40,30,50,30}, {0x243f56,0x92b7ba,0xd9ad7e,0x92b7ba}},
  {"Colour Ribbons", {nullptr, "Speed", "Ribbons", "Amplitude"}, {50,25,50,50}, {0xb05857,0xe8b987,0x468f9b,0xe8b987}},
  {"Folded Cross", {"Tile size", "Speed", "Folds", nullptr}, {50,30,50,30}, {0xdfddd1,0xdfddd1,0xdfddd1,0xdfddd1}},
  {"Bitwise Shards", {"Block size", "Speed", "Bit shift", nullptr}, {50,30,40,30}, {0x337a89,0xc0dcd1,0x337a89,0xc0dcd1}},
  {"Blooming Bioluminescence", {"Cells", "Speed", "Plasma", nullptr}, {38,38,50,0}, {0xFF4226,0x8080F3,0x000FF1,0x807303}},
  {"Chladni Acoustic Waves", {"Mode N", "Mode M", "Speed", "Edge sharpness"}, {29,36,17,37}, {0x000000,0x4500AC,0x8800C9,0xFF0000}},
  {"Pulsating Solar Flare", {"Density", "Pulse speed", "Wave height", "Glow"}, {29,19,44,50}, {0xFF0000,0xFF3900,0xFF7000,0xFFD000}},
  {"Death Star", {"Orbit speed", "Point density", "Dish size", "Equator glow"}, {25,58,38,62}, {0xbdcfc7,0xbdcfc7,0xbdcfc7,0xbdcfc7}},
  {"Pendulum Waves", {"Speed", "Base frequency", "Frequency spread", nullptr}, {12,34,50,8}, {0x8eb8cd,0xe7d5aa,0x8eb8cd,0xe7d5aa}},
  {"Nachtschwärmer", {"Flow speed", "Blob size", "Gooeyness", "Particles"}, {8,32,42,34}, {0x17535e,0x4fbaa9,0xc7ddd0,0x4fbaa9}},
  {"Cathedral Glass", {"Drift speed", "Pane size", "Lead sharpness", nullptr}, {7,60,52,34}, {0x355883,0xb96858,0xe4c784,0x559e96}},
  {"Tidal Topography", {"Tide speed", "Contour density", "Wave depth", "Line weight"}, {8,50,62,40}, {0x183e55,0x388c95,0xc5dcc2,0x388c95}},
  {"Whitney Harmonics", {"Canon speed", "Voices", "Orbit size", nullptr}, {8,64,62,48}, {0xe3d4b5,0xae6c89,0xe3d4b5,0x70adb9}},
  {}, // Retired ID 17
  {"Vega Lens", {"Grid","Bulge","Wave depth",nullptr}, {67,75,47,0}, {0xD92929,0x24A361,0x576FD6,0x1C7052}},
  {"Twisted Tunnel", {"Ring spacing","Twist",nullptr,"Travel speed"}, {50,50,50,8}, {0xD11414,0x0F478C,0xF2C20A,0xFFFFFF}},
  {}, // Retired ID 20
  {"Soliton Lattice Echo", {"Kink energy","Relative pace","Soliton count","Echo length"}, {10,10,50,83}, {0x000000,0x363636,0x737373,0xA1A1A1}},
  {"Ripple Cell Grid", {"Wave speed","Cell size","Ripple depth","Dot width"}, {50,67,50,40}, {0x000000,0xFF0000,0xEEFF00,0x000000}},
  {"Poolside", {"Pool depth","Wave drift","Chop","Swell wavelength"}, {33,26,50,0}, {0x000000,0x1100FF,0xFF0000,0xFF0000}},
  {}, // Retired ID 24
  {"Squircle Spacetime Warp", {"Circle to square","Gravity lens","Grid frequency","Cell phase"}, {65,44,29,0}, {0x000000,0x55555B,0xAAAAB0,0xFFFFFF}},
  {}, // Retired ID 26
  {}, // Retired ID 27
  {}, // Retired ID 28
  {"Hypno Circles", {nullptr,"Motion speed","Wave ratio","White intersections"}, {0,19,31,50}, {0xFF0000,0xFFFF00,0x00FFFF,0xFF00FF}},
  {}, // Retired ID 30
  {"Collatz Kelp", {"Odd turn angle","Growth speed","Even turn angle","Seed count"}, {0,36,0,0}, {0x0A7882,0x6EEB78,0xFF6E50,0xFFCD46}},
  {}, // Retired ID 32
  {"Topo Vortex", {"Ring count","Rotation speed","Twist",nullptr}, {50,23,44,50}, {0x00FF2D,0x00DCD7,0x0064FF,0x001CC3}},
  {}, // Retired ID 34
  {"Harmonic Canon", {"Canon speed","Voices","Orbit size","Trail length"}, {20,58,66,58}, {0x42bef4,0x6efbc8,0xfbda96,0xc157f2}},
  {"Chladni Resonance", {"Mode N","Mode M","Morph speed","Filament width"}, {30,57,26,40}, {0x262575,0x8e5af0,0xf56b95,0xface9a}},
  {}, // Retired ID 37
  {"Satin", {"Flow speed","Cell size","Warp strength","Fabric fold"}, {26,47,65,52}, {0x5747be,0xe76cc5,0xfac2a1,0xbc93b9}},
  {}, // Retired ID 39
  {}, // Retired ID 40
  {}, // Retired ID 41
  {"Infinite Squares", {"Travel speed","Square count","Corkscrew","Line weight"}, {28,48,46,34}, {0x50d6f8,0x7462f0,0xf55fb8,0xf9bf6b}},
  {"Motion Graphics 1968", {"Motion speed","Line count","Phase spacing",nullptr}, {50,50,50,50}, {0x07181D,0xDCD5BF,0xA34A80,0xDCD5BF}},
  {}, // Retired ID 44
  {"Chromatic Bloom", {"Bloom speed","Particle count","Petal folding","Counterpoint"}, {38,65,55,45}, {0xFBFAF8,0xF3EA83,0xC85D4D,0x4A64C7}},
  {"Abyssal Vortex", {"Ring count","Flow speed","Vortex twist","Braiding"}, {50,23,44,50}, {0x00ff2d,0x00dcd7,0x0064ff,0x001cc3}},
  {"Interference", {"Motion speed","Wavelength","Distortion","Wave interference"}, {28,24,41,55}, {0xf4475d,0xf9c45f,0x4ad0c3,0x7664f0}},
  {"Ripples", {"Expansion speed","Ring width","Spawn density","Decay"}, {64,24,55,24}, {0x54d2b6,0x55bfd8,0x476bef,0x935de8}},
  {"Luminous Loom", {"Weave speed","Ribbon count","Undulation","Ribbon width"}, {28,40,55,65}, {0xf5736d,0xfacb8e,0x4cd5c5,0x595bd1}},
  {"Ferrofluid", {"Flow speed","Droplets","Cohesion","Metallic reflection"}, {25,50,36,85}, {0x1d3c62,0x5dd6ca,0xeaf1df,0x769fb5}},
  {"Accretion", {"Orbital speed","Disc tilt","Event horizon",nullptr}, {30,34,45,55}, {0xfce09c,0xf7a458,0xd56673,0x6f56c8}},
  {"Origami Flight", {"Motion speed","Facet count","Folding depth","Precession"}, {28,35,66,55}, {0xf9be8f,0xe16e87,0x65bbc3,0xb4b3e7}},
  {"Signal Orchard", {"Pulse speed","Trees","Branching depth","Surge"}, {32,20,40,70}, {0x54f2bf,0x55c5e2,0x7979f1,0x788ac3}},
  {"Resonant Orbits", {"Canon speed","Particle count","Rate divergence","Echo length"}, {26,50,75,40}, {0xf3e2b6,0xdd8983,0x56c6c0,0x878fdf}},
  {"Phase Architecture", {"Motion speed","Strand count","Torsion","Precession"}, {26,45,65,55}, {0xeee4cb,0xd385a6,0x7bc8ce,0x989ae7}},
  {}, // Retired ID 56
  {}, // Retired ID 57
  {"Pong Wars", {"Game speed","Tile size","Ball size","Bounce variation"}, {40,33,50,25}, {0xD9E8E3,0xD9E8E3,0x172B36,0x172B36}},
  {"Square Orbits", {"Canon speed","Particle count","Rate divergence","Echo length"}, {26,65,75,40}, {0xf3e2b6,0xdd8983,0x56c6c0,0x878fdf}},
  {"Acid Cathedral", {"Motion speed","Grid density","Arch distortion","Aliasing"}, {28,32,74,55}, {0xdd6472,0xdfb673,0x60b8ac,0x8071b5}},
  {"Phase Slippage", {"Motion speed","Grid density","Phase warp","Aliasing"}, {23,63,56,82}, {0x5a699f,0x9e8bcb,0xd0b4c9,0x798fc1}},
  {"Liquid Circuit", {"Motion speed","Grid density","Liquid warp","Aliasing"}, {26,29,85,32}, {0x237d87,0x65b9a8,0xdbbd80,0x6999ae}},
  {"Raindrops", {"Drop frequency","Trail fade","Ring width","Wave speed"}, {10,20,20,25}, {0x3030FF,0x5030FF,0x7030FF,0x4030FF}},
  {"Plasma", {"Flow speed","Wave scale",nullptr,"Trail fade"}, {15,30,50,30}, {0xFF0000,0xFFFF00,0x00FFFF,0x8000FF}},
  {"Chrono Iris", {"Pulse rate","Petal count","Exposure width","Flash depth"}, {8,40,60,18}, {0x5171a6,0xa8c8d6,0xe1d9ba,0xa8c8d6}},
  {"Rolling Silk", {"Pulse rate","Curtain density","Exposure width","Flash depth"}, {6,32,65,16}, {0x3b7380,0x9ac4bb,0xddcfa5,0x9ac4bb}},
  {"Lantern Garden", {"Pulse rate","Lantern count","Exposure width","Flash depth"}, {9,35,55,20}, {0xaf704e,0xe0bb83,0xaf704e,0xe0bb83}},
};

namespace {
#if defined(MATRIX_PROFILE_WS2812) && defined(WS2812_MATRIX_WIDTH) && defined(WS2812_MATRIX_HEIGHT)
constexpr uint16_t GEOMETRY_PIXELS=WS2812_MATRIX_WIDTH*WS2812_MATRIX_HEIGHT;
#else
constexpr uint16_t GEOMETRY_PIXELS=64*64;
#endif
float polarRadius[GEOMETRY_PIXELS];
float polarAngle[GEOMETRY_PIXELS];
uint16_t geometryWidth = 0, geometryHeight = 0;
inline float unit(uint8_t value) { return value / 100.0f; }
inline float mapControl(uint8_t value, float lo, float hi) {
  return lo + (hi - lo) * unit(value);
}
inline float fract(float value) { return value - floorf(value); }
// A count control's lo..hi range at 64 pixels, thinned by scale but never below
// least, and spread over the whole control, so every part of it still counts.
inline int scaledCount(uint8_t value, int lo, int hi, int least, float scale) {
  const int a = max(least, (int)lroundf(lo * scale)), b = max(a + 1, (int)lroundf(hi * scale));
  return a + value * (b - a) / 100;
}
float fastUnitPower(float value, float exponent) {
  if (exponent == 8.f) { value*=value;value*=value;return value*value; }
  if (value <= 0.f) return 0.f;
  if (value >= 1.f) return value == 1.f ? 1.f : ::powf(value,exponent);
  static uint16_t table[513];
  static float previousExponent=-1.f;
  if (previousExponent != exponent) {
    for(int i=0;i<=512;i++)table[i]=(uint16_t)(::powf(i/512.f,exponent)*65535.f+.5f);
    previousExponent=exponent;
  }
  const float position=value*512.f;
  const int index=(int)position;
  return (table[index]+(table[index+1]-table[index])*(position-index))*(1.f/65535.f);
}
inline float softExpNeg(float value) {
  const float inverse = 1.0f / (1.0f + max(0.0f, value) * 0.25f);
  const float squared = inverse * inverse;
  return squared * squared;
}

// An aliased line whose palette position runs from hueA to hueB. Pixels off the
// canvas are skipped, and a line longer than four canvas spans is not drawn.
IRAM_ATTR void paletteLine(PatternflowPixelWriter writer, void* context, int width, int height,
                 const uint16_t palette[256], float ax, float ay, float bx, float by,
                 float hueA, float hueB, float value) {
  int x0=roundInt(ax),y0=roundInt(ay);
  const int x1=roundInt(bx),y1=roundInt(by);
  const int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,steps=max(dx,-dy);
  if (value <= 0.005f || steps > 4*(width+height)) return;
  const float hueStep=(hueB-hueA)*fastReciprocal(float(steps));
  for (int step=0,error=dx+dy;;step++) {
    if (x0>=0 && y0>=0 && x0<width && y0<height)
      writer(context,x0,y0,paletteColorAt(palette,hueA+hueStep*step,value));
    if (x0==x1 && y0==y1) break;
    const int twice=2*error;
    if (twice>=dy) { error+=dy; x0+=sx; }
    if (twice<=dx) { error+=dx; y0+=sy; }
  }
}

// Additive RGB trails in 16-bit channels: 2114 per red or blue step and 1040
// per green step, so slow fades keep their hue without banding. The palette's
// brightest channels cap them, so zero brightness turns trails dark at once.
struct TrailCap { int r, g, b; };
TrailCap trailCap(const uint16_t palette[256]) {
  TrailCap cap={0,0,0};
  for (int i=0;i<256;i++) {
    cap.r=max(cap.r,(palette[i]>>11)*2114);
    cap.g=max(cap.g,((palette[i]>>5)&63)*1040);
    cap.b=max(cap.b,(palette[i]&31)*2114);
  }
  return cap;
}
void trailFade(uint16_t* trail, int channels, uint32_t keep) {
  for (int i=0;i<channels;i++) trail[i]=(uint32_t(trail[i])*keep)>>8;
}
// One shared limiter keeps the blended hue; clipping each channel on its own
// would turn overlaps white.
IRAM_ATTR void trailAdd(uint16_t* rgb, uint16_t color, float amount, const TrailCap& cap) {
  const int r=rgb[0]+int((color>>11)*2114*amount),g=rgb[1]+int(((color>>5)&63)*1040*amount),b=rgb[2]+int((color&31)*2114*amount);
  float gain=1.f;
  auto limit=[&](int channel,int most) PIXEL_INLINE {
    if (channel>most) { const float share=most*fastReciprocal(float(channel)); if (share<gain) gain=share; }
  };
  limit(r,cap.r);limit(g,cap.g);limit(b,cap.b);
  rgb[0]=uint16_t(r*gain);rgb[1]=uint16_t(g*gain);rgb[2]=uint16_t(b*gain);
}
IRAM_ATTR uint16_t trailColor(const uint16_t* rgb, const TrailCap& cap) {
  return rgb565(min(cap.r,int(rgb[0]))*(1.f/65534),min(cap.g,int(rgb[1]))*(1.f/65520),min(cap.b,int(rgb[2]))*(1.f/65534));
}

inline uint32_t nextRandom(uint32_t& state) {
  state = state * 1664525UL + 1013904223UL;
  return state;
}

inline float randomUnit(uint32_t& state) {
  return (nextRandom(state) >> 8) / 16777216.0f;
}

// A fixed 0..1 value for each n.
inline float hashUnit(uint32_t n) {
  n *= 2654435761U; n ^= n >> 15; n *= 2246822519U; n ^= n >> 13;
  return (n & 0xFFFFFF) * (1.0f / 16777216.0f);
}

// Whole steps of a simulation at rate per second due after dt. The carried
// remainder keeps motion the same at any frame rate; carries start at half a
// step, so a steady frame clock stays clear of step boundaries instead of
// jittering between none and two.
constexpr float FIRST_CARRY = .5f;
int fixedSteps(float& carry, float dt, float rate) {
  carry += dt * rate;
  const int steps = (int)(carry + 1e-5f);
  carry = max(0.f, carry - steps);
  return steps;
}
}

#include "PatternflowImports.inc"
#include "PatternRemixes.inc"
#include "WhitneyStudies.inc"
#include "AdvancedStudies.inc"
#include "NovaStudies.inc"
#include "AcidStudies.inc"
#include "PongWars.inc"
#include "Classic08.inc"
#include "FlashingStudies.inc"

void PatternflowPack::reset(uint8_t index) {
  if (index < PACK_CLOCKS) time_[index] = 0.0f;
  pendulumPhase_[0] = pendulumPhase_[1] = 0.0f;
  resetImportedPattern();
  resetRemixPattern();
  resetWhitneyPattern();
  resetAdvancedPattern();
  resetPongWars();
  resetClassic08();
  resetFlashingStudies();
}

void PatternflowPack::render(uint8_t index, uint16_t width, uint16_t height,
                             float deltaTime, const uint8_t p[4],
                             const uint16_t palette[256], const SourceColors& sourceColors,
                             PatternflowPixelWriter writer, void* context) {
  if (index >= PATTERNFLOW_PATTERN_COUNT || !PATTERNFLOW_METADATA[index].name ||
      !writer || width == 0 || height == 0) return;
  ensureFastMath();
  if (width > 64 || height > 64 || (uint32_t)width*height > GEOMETRY_PIXELS) return;
  // A stalled frame advances every pattern by at most a tenth of a second.
  deltaTime = constrain(deltaTime, 0.f, .1f);
  if (geometryWidth != width || geometryHeight != height) {
    const float centerX=(width-1)*.5f, centerY=(height-1)*.5f;
    const float inverseHeight=1.f/height;
    for (uint16_t y=0;y<height;y++) for(uint16_t x=0;x<width;x++) {
      const uint16_t pixel=y*width+x;
      const float dx=(x-centerX)*inverseHeight,dy=(y-centerY)*inverseHeight;
      polarRadius[pixel]=fastHypot(dx,dy);
      polarAngle[pixel]=fastAtan2(dy,dx);
    }
    geometryWidth=width;geometryHeight=height;
  }
  if(index >= 65) { renderFlashingStudy(index-65,width,height,deltaTime,p,palette,writer,context); return; }
  if(index >= 63) { renderClassic08(index-63,width,height,deltaTime,p,palette,writer,context); return; }
  if(index >= 60) { renderAcidPattern(index-60,width,height,deltaTime,p,palette,writer,context); return; }
  if(index == 59) { renderNovaPattern(7,width,height,deltaTime,p,palette,writer,context); return; }
  if(index == 58) { renderPongWars(width,height,deltaTime,p,palette,writer,context); return; }
  if(index >= 49) { renderNovaPattern(index-49,width,height,deltaTime,p,palette,writer,context); return; }
  if(index >= 46) { renderAdvancedPattern(index-46,width,height,deltaTime,p,palette,writer,context); return; }
  if(index >= 43) { renderWhitneyPattern(index-43,width,height,deltaTime,p,palette,sourceColors,writer,context); return; }
  if(index >= 35) { renderRemixPattern(index-35,width,height,deltaTime,p,palette,writer,context); return; }
  if(index >= 17) { renderImportedPattern(index-17,width,height,deltaTime,p,palette,sourceColors,writer,context); return; }
  float& t = time_[index];
  const float cx = (width - 1) * 0.5f, cy = (height - 1) * 0.5f;
  const float aspect = width / (float)height;
  const float inverseWidth=1.f/width,inverseHeight=1.f/height;
  const float inverseEdgeWidth=1.f/max(1,(int)width-1);
  const float inverseEdgeHeight=1.f/max(1,(int)height-1);
  auto put = [&](int x, int y, float hue, float value = 1.0f) PIXEL_INLINE {
    if (value > 0.005f && x >= 0 && y >= 0 && x < width && y < height)
      writer(context, x, y, paletteColorAt(palette, hue, value));
  };
  auto putRgb=[&](int x,int y,float r,float g,float b) PIXEL_INLINE {
    if (x >= 0 && y >= 0 && x < width && y < height)
      writer(context,x,y,sourceColor(sourceColors,(int)r,(int)g,(int)b));
  };
  struct Point3 { float x, y, z; };
  struct ScreenPoint { float x, y, z; };
  auto rotate3 = [](Point3 v, float ax, float ay, float az) {
    const float sx=fastSin(ax),cx=fastCos(ax),sy=fastSin(ay),cy=fastCos(ay),sz=fastSin(az),cz=fastCos(az);
    const float y1=v.y*cx-v.z*sx,z1=v.y*sx+v.z*cx;
    const float x2=v.x*cy+z1*sy,z2=-v.x*sy+z1*cy;
    return Point3{x2*cz-y1*sz,x2*sz+y1*cz,z2};
  };
  auto drawLine = [&](ScreenPoint a, ScreenPoint b, float hueA, float hueB, float value) {
    paletteLine(writer, context, width, height, palette, a.x, a.y, b.x, b.y, hueA, hueB, value);
  };
  auto ambientSpeed = [](uint8_t value, float slow, float fast) {
    float n=value/100.f;n=n*n*(3.f-2.f*n);return slow+(fast-slow)*n;
  };

  switch (index) {
    case 0: { // Vector Field Flow
      const float turbulence = unit(p[0]);
      t += deltaTime * mapControl(p[1], 0.2f, 6.0f);
      const float density = mapControl(p[2], 1.0f, 20.0f);
      for (int y=0; y<height; y++) for (int x=0; x<width; x++) {
        const float nx=(x*inverseWidth-.5f)*aspect, ny=y*inverseHeight-.5f;
        const float angle=fastSin(nx*density+t)+fastCos(ny*density-t);
        const float vx=fastSin(angle*turbulence*5), vy=fastCos(angle*turbulence*5);
        const float v=.5f+.5f*fastSin((nx*vx+ny*vy)*10+t*2);
        put(x,y,unit(p[3])+angle*.16f+vx*.2f,v);
      } break;
    }
    case 1: { // Chromatic Vortex
      t += deltaTime * mapControl(p[1],.2f,5.f);
      const float split=unit(p[0])*2.5f, density=mapControl(p[2],3.f,18.f);
      for(int y=0;y<height;y++) for(int x=0;x<width;x++) {
        const float r=polarRadius[y*width+x],a=polarAngle[y*width+x];
        const float bands=.5f+.5f*fastSin(r*density-t*2+a+split*fastSin(r*9-t));
        put(x,y,a/(2*PI)+r+unit(p[3])*.45f,bands);
      } break;
    }
    case 2: { // Liquid Ripple
      t += deltaTime*mapControl(p[1],.1f,4.f);
      const float refract=mapControl(p[0],0,4),waves=mapControl(p[2],4,24),spec=unit(p[3]);
      for(int y=0;y<height;y++) for(int x=0;x<width;x++) {
        const float dx=(x-cx)*inverseHeight,dy=(y-cy)/(float)height,r=fastSqrt(dx*dx+dy*dy);
        const float ripple=fastSin(r*waves-t*3+fastSin((dx-dy)*7+t)*refract);
        put(x,y,.52f+ripple*.12f+t*.01f,clamp01(.35f+.45f*ripple+spec*fastUnitPower(fabsf(ripple),8)));
      } break;
    }
    case 3: { // Kaleidoscope
      t += deltaTime*mapControl(p[1],.1f,4.f);
      const int symmetry=3+(int)(unit(p[0])*9); const float twist=mapControl(p[3],0,5),depth=mapControl(p[2],2,18);
      for(int y=0;y<height;y++) for(int x=0;x<width;x++) {
        const float r=polarRadius[y*width+x],a=polarAngle[y*width+x];
        const float folded=fabsf(fract((a/(2*PI)+.5f)*symmetry)*2-1);
        const float v=.5f+.5f*fastSin(r*depth-folded*PI*2+t*2+twist*r);
        put(x,y,folded+r*.4f+t*.015f,v);
      } break;
    }
    case 4: { // Isometric Pillars
      t += deltaTime*mapControl(p[1],.1f,4.f)*1.5f;
      // Cells shrink less than the panel, keeping their faces apart, and the height
      // waves keep their count across the panel.
      const float panel=min(width,height)/64.f;
      const int cell64=6-(int)(unit(p[0])*4),cell=max(2,(int)lroundf(cell64*(.5f+.5f*panel)));
      const float shift=unit(p[3]),inverseCell=1.f/cell,waves=.8f*cell/(cell64*panel);
      for(int y=0;y<height;y++) for(int x=0;x<width;x++) {
        const float along=(x+y-cx-cy)*inverseCell+.5f; const int gx=floorInt(along),gy=floorInt((y-x-cy+cx)*inverseCell+.5f); const float rise=.5f+.5f*fastSin(fastHypot(gx,gy)*waves-t);
        const float local=along-gx; const float face=local<1.f/3?1.f:(local<2.f/3?.65f:.38f);
        // Low pillars take the palette's body, the tallest its accent.
        put(x,y,shift+rise*.45f,face*(.25f+rise*.75f));
      } break;
    }
    case 5: { // Colour Ribbons
      t += deltaTime*mapControl(p[1],.1f,4.f)*2;
      const int ribbons=2+(int)(unit(p[2])*8); const float k64=64.f/min(width,height),amp=mapControl(p[3],2,height*.45f);
      float ribbonY[64][10];
      for(int x=0;x<width;x++)for(int i=0;i<ribbons;i++)ribbonY[x][i]=cy+(fastSin(x*k64*.18f+t+i*1.337f)+fastCos(x*k64*.27f-t*.8f+i*1.337f))*amp;
      // Ribbons spread from the palette's body to its accent; where they meet,
      // their colours blend by glow.
      float ribbonHue[10];for(int i=0;i<ribbons;i++)ribbonHue[i]=.2f+.6f*i/(ribbons-1);
      for(int y=0;y<height;y++) for(int x=0;x<width;x++) {float glow=0,hue=0;for(int i=0;i<ribbons;i++){const float d=fabsf(y-ribbonY[x][i])*k64;if(d<4){const float g=fastUnitPower(1/(d+1),1.5f);glow+=g;hue+=g*ribbonHue[i];}}if(glow>.1f)put(x,y,hue/glow,clamp01(glow*.8f));} break;
    }
    case 6: { // Folded Cross
      t += deltaTime*mapControl(p[1],.1f,4.f);
      // Folds rise from 1.5, where tiles still vary across the panel, to 2.45 at 50
      // and on to 4.9.
      const int fold=4+(int)(unit(p[0])*10); const float u=unit(p[2]),limit=u<.5f?1.5f+1.9f*u:4.9f*u;
      float foldX[64],foldY[64];for(int x=0;x<width;x++)foldX[x]=fabsf(fmodf(fabsf(x-cx),fold*2.f)-fold)*limit;for(int y=0;y<height;y++)foldY[y]=fabsf(fmodf(fabsf(y-cy),fold*2.f)-fold)*limit;
      for(int y=0;y<height;y++) for(int x=0;x<width;x++){const float fx=foldX[x],fy=foldY[y],c=fabsf(fastSin(fx*.2f+t)*fastCos(fy*.2f-t)+fastCos(fx*.1f-t*1.5f)*fastSin(fy*.1f+t));if(c>.65f)put(x,y,.9f,clamp01((c-.65f)/.35f));else if(c<.08f)put(x,y,.16f,.65f);} break;
    }
    case 7: { // Bitwise Shards
      t += deltaTime*mapControl(p[1],.1f,4.f);
      // Blocks shrink with the square root of the panel, so a smaller one shows more
      // of them, and each shard keeps its banding.
      const int block64=2+(int)(unit(p[0])*6),block=max(2,(int)lroundf(block64*sqrtf(min(width,height)/64.f))),shift=(int)mapControl(p[2],1,5);
      const float base=unit(p[3]),banding=.3f*block64/block;
      for(int y=0;y<height;y++) for(int x=0;x<width;x++){const int bx=y/block,ax=x/block,mx=abs(x%(block*2)-block),my=abs(y%(block*2)-block),id=(ax^bx)<<shift;const float v=fabsf(fastSin((mx+my)*banding+id*.1f+t));if(v>.6f)put(x,y,base+(ax^bx)*.07f,(v-.6f)/.4f);else if(v<.05f)put(x,y,base+.5f,.15f);} break;
    }
    case 8: { // Blooming Bioluminescence by @WollyMeister
      static const float orbitA[10] = {1.30f,.71f,1.13f,.92f,1.47f,.63f,1.21f,.84f,1.38f,1.05f};
      static const float orbitB[10] = {.94f,1.23f,.81f,1.41f,.73f,1.32f,1.02f,1.51f,.62f,1.17f};
      static const float orbitPhase[10] = {0,.63f,1.26f,1.89f,2.51f,3.14f,3.77f,4.40f,5.03f,5.65f};
      static const float orbitRadius[10] = {.52f,.47f,.60f,.41f,.56f,.50f,.44f,.58f,.49f,.55f};
      const int cells = constrain((int)roundf(mapControl(p[0], 2, 10)), 2, 10);
      t += deltaTime * mapControl(p[1], .1f, 3.f);
      const float plasmaStrength = mapControl(p[2], 0, 3.f);
      const float baseHue = unit(p[3]);
      float cellX[10], cellY[10], pulse[10];
      for (int i=0; i<cells; i++) {
        cellX[i] = fastSin(t*orbitA[i]+orbitPhase[i])*orbitRadius[i]*aspect;
        cellY[i] = fastCos(t*orbitB[i]+orbitPhase[i]+1.f)*orbitRadius[i];
        pulse[i] = .88f+.12f*fastSin(t*2.1f+orbitPhase[i]*3.7f);
      }
      for (int y=0; y<height; y++) for (int x=0; x<width; x++) {
        const float nx=(x*inverseEdgeWidth-.5f)*2.f*aspect;
        const float ny=(y*inverseEdgeHeight-.5f)*2.f;
        float f1sq=1e9f,f2sq=1e9f; int nearest=0;
        for (int i=0; i<cells; i++) {
          const float dx=nx-cellX[i],dy=ny-cellY[i];
          const float d2=(dx*dx+dy*dy)*pulse[i]*pulse[i];
          if(d2<f1sq){f2sq=f1sq;f1sq=d2;nearest=i;}else if(d2<f2sq)f2sq=d2;
        }
        const float f1=fastSqrt(f1sq), border=max(0.f,fastSqrt(f2sq)-f1);
        const float radius=fastSqrt(nx*nx+ny*ny);
        const float plasma=(fastSin(nx*7+t*1.3f)+fastSin(ny*6-t)+
          fastSin((nx+ny)*5+t*.8f)+fastSin(radius*9-t*1.7f))*.04f*plasmaStrength;
        const float distance=max(0.f,f1+plasma);
        const float glow=softExpNeg(distance*4.f),core=softExpNeg(distance*14.f);
        const float rim=softExpNeg(border*9.f)*.7f;
        const float brightness=clamp01(glow*1.1f+rim+core*.45f);
        const float hue=baseHue+nearest/(float)cells+border*.3f+plasma*.12f+t*.03f;
        if(brightness>.012f)putRgb(x,y,(.5f+.5f*fastCos(hue*2*PI))*brightness*255,
          (.5f+.5f*fastCos((hue*1.1f+.33f)*2*PI))*brightness*255,
          (.5f+.5f*fastCos((hue*.9f+.67f)*2*PI))*brightness*255);
      } break;
    }
    case 9: { // Chladni Acoustic Waves by @engmung
      const float n=mapControl(p[0],1,8),m=mapControl(p[1],1,12);
      t += deltaTime*mapControl(p[2],.2f,2.f);
      const float sharp=mapControl(p[3],1,20);
      const float nMod=n+fastSin(t)*.5f,mMod=m+fastCos(t*.8f)*.5f;
      for(int y=0;y<height;y++)for(int x=0;x<width;x++){
        const float nx=(x*inverseEdgeWidth-.5f)*2.f;
        const float ny=(y*inverseEdgeHeight-.5f)*2.f;
        const float field=fastCos(nMod*PI*nx)*fastCos(mMod*PI*ny)-
                          fastCos(mMod*PI*nx)*fastCos(nMod*PI*ny);
        const float node=softExpNeg(fabsf(field)*sharp);
        if(node>.002f){float low=clamp01(node/.49f),high=clamp01((node-.49f)/.51f);
          putRgb(x,y,102*low+153*high,0,255*low*(1-high));}
      } break;
    }
    case 10: { // Pulsating Solar Flare by @martinnlopez
      // Below 32 pixels dots keep half their 64-pixel spacing, and fewer of them
      // fill the same share of the panel.
      const float panel=min(width,height)/64.f,sizeScale=max(.5f,panel);
      const int count=max(12,(int)roundf(mapControl(p[0],50,400)*panel*panel/(sizeScale*sizeScale)));
      t += deltaTime*mapControl(p[1],.1f,10.f);
      const float amplitude=mapControl(p[2],1,10),glow=unit(p[3]);
      const float goldenAngle=2.39996f;
      for(int i=1;i<count;i++){
        const float baseRadius=3.f*fastSqrt(i)*sizeScale;
        const float pulse=fastSin(baseRadius/max(.1f,sizeScale)*.2f-t)*amplitude*sizeScale;
        const float radius=baseRadius+pulse,angle=i*goldenAngle+t*.2f;
        const int x=(int)lroundf(cx+fastCos(angle)*radius),y=(int)lroundf(cy+fastSin(angle)*radius);
        if(x<0||x>=width||y<0||y>=height)continue;
        const float value=clamp01(.58f+.42f*fastSin(pulse/max(.1f,sizeScale)*.5f));
        const float hue=.96f+radius/max(1.f,(float)min(width,height))*.16f+t*.008f;
        putRgb(x,y,value*255,value*glow*255,0);
        if(glow>.5f){
          const float bloom=value*glow*(glow-.5f);
          if(x+1<width)putRgb(x+1,y,bloom*255,0,0);
          if(y+1<height)putRgb(x,y+1,bloom*255,0,0);
          if(glow>.65f&&x>0)put(x-1,y,hue+.02f,bloom*.6f);
        }
      } break;
    }
    case 11: { // Death Star, after PsiOp by aap (MIT): patterns/imports/psiop/LICENSE
      t += deltaTime*mapControl(p[0],.08f,1.6f);
      const float lattice=max(.3f,min(width,height)/64.f);
      const int latitudes=scaledCount(p[1],10,28,4,lattice),longitudes=scaledCount(p[1],16,44,8,lattice);
      const float dishRadius=mapControl(p[2],.18f,.52f);
      const float equatorGlow=mapControl(p[3],.25f,1.f);
      const float radius=min(width,height)*.39f;
      const float az=-t*.72f,tilt=.22f+.12f*fastSin(t*.23f);
      auto projectStar=[&](Point3 v){
        const Point3 q=rotate3(v,tilt,0,az);
        return ScreenPoint{cx+q.x*radius,cy+q.y*radius,q.z};
      };
      for(int j=1;j<latitudes;j++){
        const float latitude=-PI*.5f+PI*j/latitudes;
        const float cl=fastCos(latitude),sl=fastSin(latitude);
        for(int i=0;i<longitudes;i++){
          const float longitude=2*PI*i/longitudes;
          const Point3 local={cl*fastCos(longitude),cl*fastSin(longitude),sl};
          const bool insideDish=local.x>.65f &&
            local.y*local.y+local.z*local.z<dishRadius*dishRadius;
          if(!insideDish){
            const ScreenPoint q=projectStar(local);
            put((int)roundf(q.x),(int)roundf(q.y),.58f+.24f*q.z+t*.012f,
                .48f+.48f*(q.z*.5f+.5f));
          }
        }
      }
      ScreenPoint previous={};
      for(int i=0;i<=longitudes;i++){
        const float a=2*PI*(i%longitudes)/longitudes;
        const ScreenPoint q=projectStar({fastCos(a),fastSin(a),0});
        if(i)drawLine(previous,q,.47f,.68f,equatorGlow);
        previous=q;
      }
      const int dishSegments=max(12,longitudes/2);
      for(int ring=0;ring<2;ring++){
        const float rr=dishRadius*(ring? .66f:1.f);
        for(int i=0;i<=dishSegments;i++){
          const float a=2*PI*(i%dishSegments)/dishSegments;
          const float x=fastSqrt(max(0.f,1.f-rr*rr));
          const ScreenPoint q=projectStar({x,rr*fastCos(a),rr*fastSin(a)});
          if(i)drawLine(previous,q,.02f+.15f*ring,.12f+.15f*ring,.95f);
          previous=q;
        }
      }
      break;
    }
    case 12: { // Pendulum Waves
      const float step=deltaTime*ambientSpeed(p[0],.22f,4.2f);
      t+=step;
      // Column k swings at base + k·spread, integrated so a moved control never jumps.
      pendulumPhase_[0]+=step*mapControl(p[1],.75f,5.5f);
      pendulumPhase_[1]+=step*mapControl(p[2],.05f,6.f);
      const float margin=mapControl(p[3],0.f,min(width,height)*.32f);
      const float amplitude=max(.2f,cy-margin);
      ScreenPoint previous={0,0,0};
      for(int x=0;x<width;x++){
        const float k=width>1?x/(float)(width-1):0;
        const float y=cy+amplitude*fastSin(pendulumPhase_[0]+k*pendulumPhase_[1]);
        const ScreenPoint q={(float)x,y,0};
        if(x)drawLine(previous,q,k*.45f+t*.012f,k*.45f+t*.012f,.78f);
        put(x,(int)roundf(y),k*.45f+t*.012f,1.f);
        previous=q;
      }break;
    }
    case 13: { // Nachtschwärmer metaballs
      t+=deltaTime*ambientSpeed(p[0],.16f,3.2f);
      // Distances count in sixteenths of the panel, so blobs keep their share of any size.
      const float radius=mapControl(p[1],2.f,9.6f),unitSize=min(width,height)/16.f;
      const float threshold=mapControl(p[2],5.2f,1.25f);
      const int balls=constrain((int)roundf(mapControl(p[3],1.f,8.f)),1,8);
      // radius/max(.22, d/unit) as radius·unit/max(.22·unit, d), without a division per ball.
      const float strength=radius*unitSize,nearest=.22f*unitSize;
      float bx[8],by[8];
      for(int i=0;i<balls;i++){
        bx[i]=cx+fastSin(t*(.71f+i*.073f)+i*2.17f)*width*(.18f+.018f*i);
        by[i]=cy+fastCos(t*(.63f+i*.061f)+i*1.43f)*height*(.17f+.016f*i);
      }
      for(int y=0;y<height;y++)for(int x=0;x<width;x++){
        float field=0;
        for(int i=0;i<balls;i++)field+=strength*fastReciprocal(max(nearest,fastDistance(x-bx[i],y-by[i])));
        if(field>threshold){const float v=clamp01((field-threshold)*.72f+.12f);
          // Rims take the palette's body and cores rise towards its accent.
          put(x,y,.22f+min(.5f,(field-threshold)*.1f)+t*.008f,v);}
      }break;
    }
    case 14: { // Cathedral Glass
      t+=deltaTime*ambientSpeed(p[0],.14f,2.2f);
      const float cellScale=mapControl(p[1],.6f,1.25f);
      const float lead=mapControl(p[2],.18f,.95f);
      const float spread=mapControl(p[3],.03f,.28f);
      constexpr int seeds=7;float sx[seeds],sy[seeds];
      for(int i=0;i<seeds;i++){
        sx[i]=cx+fastSin(t*(1.1f+i*.07f)+i*2.11f)*(width*.32f*cellScale)+fastCos(i*1.7f)*2;
        sy[i]=cy+fastCos(t*(.9f+i*.05f)+i*1.73f)*(height*.32f*cellScale)+fastSin(i*2.3f)*2;
      }
      // Leads measure squared distance in 64-pixel units, so they keep their width.
      const float area=4096.f/(min(width,height)*min(width,height));
      for(int y=0;y<height;y++)for(int x=0;x<width;x++){
        float best=1e9f,second=1e9f;int nearest=0;
        for(int i=0;i<seeds;i++){const float dx=x-sx[i],dy=y-sy[i],d=dx*dx+dy*dy;
          if(d<best){second=best;best=d;nearest=i;}else if(d<second)second=d;}
        const float edge=clamp01((second-best)*area*lead*.035f);
        const float pane=.55f+.45f*fastSin(best*.075f+t*1.4f+nearest);
        put(x,y,nearest*spread+pane*.025f+t*.006f,
            clamp01(.14f+pane*.38f+(1.f-edge)*.46f));
      }break;
    }
    case 15: { // Tidal Topography
      t+=deltaTime*ambientSpeed(p[0],.16f,2.6f);
      // A smaller panel keeps more of the contours, about three pixels apart at
      // 16, and its swell keeps its waves across the panel.
      const float panel=min(width,height)/64.f,swell=1.f/sqrtf(panel);
      const float spacing=mapControl(p[1],.38f,1.05f)*sqrtf(swell);
      const float depth=mapControl(p[2],.2f,.95f);
      const float weight=mapControl(p[3],1.9f,1.35f);
      const float fx=.42f*swell,fy=.38f*swell,fd=.19f*swell,reach=.003f/panel;
      for(int y=0;y<height;y++)for(int x=0;x<width;x++){
        const float r=fastHypot(x-cx,y-cy);
        float field=r*spacing+fastSin(x*fx+t*3.1f)*depth+
          fastSin(y*fy-t*2.2f)*depth*.75f+fastSin((x+y)*fd+t*1.3f)*depth*.55f;
        const float line=1.f-clamp01(fabsf(fastSin(field))*weight);
        const float water=.5f+.5f*fastSin(field*.31f-t*1.7f);
        // Water swells between the palette's body and highlight; contour lines
        // run from highlight to accent.
        put(x,y,.14f+.3f*line+.26f*water+r*reach,clamp01(.1f+water*.28f+line*.57f));
      }break;
    }
    case 16: { // Whitney Harmonics
      t+=deltaTime*ambientSpeed(p[0],.16f,3.f);
      const int voices=constrain((int)roundf(mapControl(p[1],5.f,13.f)),5,13);
      const float radius=mapControl(p[2],min(width,height)*.15f,min(width,height)*.44f);
      const int ghosts=constrain((int)roundf(mapControl(p[3],1.f,4.f)),1,4);
      for(int ghost=ghosts-1;ghost>=0;ghost--){
        const float gt=t-ghost*.035f;ScreenPoint previous={0,0,0};
        for(int i=0;i<voices;i++){
          const float k=i/(float)max(1,voices-1),phase=gt*(1.f+i*.1618f)*2*PI+i*.55f;
          const float rr=radius*(.18f+.82f*k);
          const ScreenPoint q={cx+fastCos(phase)*rr+fastSin(phase*.381966f+gt*1.7f)*1.6f,
                          cy+fastSin(phase*1.618034f)*rr*.72f+fastCos(phase*.5f)*1.4f,0};
          put((int)roundf(q.x),(int)roundf(q.y),i/(float)voices+t*.01f,
              (1.f-ghost/(float)(ghosts+1))*.92f);
          if(i)drawLine(previous,q,(i-1)/(float)voices,i/(float)voices,.24f/(ghost+1));
          previous=q;
        }
      }break;
    }
    default: break;
  }
}
