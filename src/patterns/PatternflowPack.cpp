#include "PatternflowPack.h"

#include <Arduino.h>
#include <math.h>

// ESP32 has float multiply/add hardware but division is a software routine.
// Permit reciprocal factoring and inline the shader helpers; retain NaN/Inf
// checks (do not enable finite-math-only / the blanket -ffast-math option).
#pragma GCC optimize ("O3", "no-math-errno", "unsafe-math-optimizations")

// Arduino's PI macro is a double. On Xtensa that silently promotes shader
// expressions to software-emulated double arithmetic; keep this renderer float.
#undef PI
#define PI 3.14159265358979323846f

const PatternflowMetadata PATTERNFLOW_METADATA[PATTERNFLOW_PATTERN_COUNT] = {
  {"Vector Field Flow", {"Turbulence", "Velocity", "Density", "Palette"}, {50,35,50,10}, {0xFF3155,0xFFC247,0x22D3C5,0x765DFF}},
  {"Chromatic Vortex", {"Split", "Swirl speed", "Density", "Color bias"}, {40,25,40,50}, {0xFF3155,0xFFC247,0x22D3C5,0x765DFF}},
  {"Liquid Ripple", {"Refraction", "Speed", "Waves", "Specular"}, {40,40,50,20}, {0xFF3155,0xFFC247,0x22D3C5,0x765DFF}},
  {"PF Kaleidoscope", {"Symmetry", "Speed", "Core depth", "Twist"}, {30,30,50,40}, {0xFF3155,0xFFC247,0x22D3C5,0x765DFF}},
  {"Isometric Pillars", {"Grid size", "Speed", "Height", "Color shift"}, {40,30,50,30}, {0xFF3155,0xFFC247,0x22D3C5,0x765DFF}},
  {"Color Ribbons", {"Hue", "Speed", "Ribbons", "Amplitude"}, {50,25,50,50}, {0xFF3155,0xFFC247,0x22D3C5,0x765DFF}},
  {"Folded Cross", {"Density", "Speed", "Folds", "Color mask"}, {50,30,50,30}, {0xFF3155,0xFFC247,0x22D3C5,0x765DFF}},
  {"Bitwise Shards", {"Resolution", "Speed", "Bit shift", "Palette"}, {50,30,40,30}, {0xFF3155,0xFFC247,0x22D3C5,0x765DFF}},
  {"Blooming Bioluminescence", {"Cells", "Speed", "Plasma", "Base hue"}, {38,38,50,0}, {0xFF4226,0x8080F3,0x000FF1,0x807303}},
  {"Chladni Acoustic Waves", {"Mode N", "Mode M", "Speed", "Edge sharpness"}, {29,36,17,37}, {0x000000,0x4500AC,0x8800C9,0xFF0000}},
  {"Pulsating Solar Flare", {"Density", "Pulse speed", "Wave frequency", "Glow"}, {29,19,44,50}, {0xFF0000,0xFF3900,0xFF7000,0xFFD000}},
  {"PsiOp Death Star", {"Orbit speed", "Point density", "Dish size", "Equator glow"}, {25,58,38,62}, {0xFF3155,0xFFC247,0x22D3C5,0x765DFF}},
  {"Pendulum Waves", {"Animation speed", "Base frequency", "Frequency spread", "Margin"}, {12,34,50,8}, {0xFF3155,0xFFC247,0x22D3C5,0x765DFF}},
  {"Nachtschwärmer", {"Flow speed", "Blob size", "Gooeyness", "Particles"}, {8,32,42,34}, {0xFF3155,0xFFC247,0x22D3C5,0x765DFF}},
  {"Cathedral Glass", {"Drift speed", "Pane size", "Lead glow", "Color spread"}, {7,60,52,34}, {0xFF3155,0xFFC247,0x22D3C5,0x765DFF}},
  {"Tidal Topography", {"Tide speed", "Contour spacing", "Wave depth", "Line weight"}, {8,50,62,40}, {0xFF3155,0xFFC247,0x22D3C5,0x765DFF}},
  {"Whitney Harmonics", {"Canon speed", "Voices", "Orbit size", "Persistence"}, {8,64,62,48}, {0xFF3155,0xFFC247,0x22D3C5,0x765DFF}},
  {"Velvet Steps", {"Walk","Direction","Cat scale","Frost"}, {30,0,42,25}, {0xEBECE9,0x595C5F,0xBA7B8B,0xE6BEC3}},
  {"Vega444 C", {"Grid","Bulge","Wave depth","Color drift"}, {67,75,47,0}, {0xD92929,0x24A361,0x576FD6,0x1C7052}},
  {"Twisted Tunnel", {"Ring spacing","Twist","Accent hue","Travel speed"}, {50,50,50,8}, {0xD11414,0x0F478C,0xF2C20A,0xFFFFFF}},
  {"TV Setup", {"Channel tuning","Interference","Snow","RGB convergence"}, {0,12,0,50}, {0xFFFFFF,0xFFFF00,0x00FFFF,0xFF00FF}},
  {"Soliton Lattice Echo", {"Kink energy","Relative pace","Soliton count","Echo decay"}, {10,10,50,83}, {0x000000,0x363636,0x737373,0xA1A1A1}},
  {"Ripple Cell Grid 2", {"Wave speed","Cell size","Ripple depth","Dot width"}, {50,67,50,40}, {0x000000,0xFF0000,0xEEFF00,0x000000}},
  {"Poolside", {"Pool depth","Wave drift","Chop","Swell wavelength"}, {33,26,50,0}, {0x000000,0x1100FF,0xFF0000,0xFF0000}},
  {"Pareidolia", {"Drift speed","Noise detail","Ink density","Domain warp"}, {19,50,50,48}, {0x212865,0x000000,0x00F900,0x000000}},
  {"Squircle Spacetime Warp", {"Circle to square","Gravity lens","Grid frequency","Cell phase"}, {65,44,29,0}, {0x000000,0x55555B,0xAAAAB0,0xFFFFFF}},
  {"Mondrian", {"Accent hue","Hold time","Cell count","Move speed"}, {50,35,27,32}, {0xFFFAEA,0xD11414,0x0F478C,0xF2C20A}},
  {"Midnight Metropolis", {"Scroll speed","City mood","Haze","Window activity"}, {29,45,50,39}, {0x10143E,0xFEF032,0xFF3C5F,0x00EBD7}},
  {"Mercury Rain", {"Rain rate","Ripple size","Expansion speed","Chrome contrast"}, {26,35,35,29}, {0x000000,0x555555,0xAAAAAA,0xFFFFFF}},
  {"Hypno Circles", {"Spectrum center","Motion speed","Wave ratio","White intersections"}, {0,19,31,50}, {0xFF0000,0xFFFF00,0x00FFFF,0xFF00FF}},
  {"Entropy Glass", {"Neck width","Sorting demon","Gravity","Color blindness"}, {0,100,0,0}, {0xFFF0CD,0xFF4E34,0xAA3CFF,0x00CDBE}},
  {"Collatz Kelp", {"Odd turn angle","Growth speed","Even turn angle","Seed count"}, {0,36,0,0}, {0x0A7882,0x6EEB78,0xFF6E50,0xFFCD46}},
  {"Bus Window Rain", {"Rain rate","Fog regrowth","Bus speed","Light focus"}, {30,30,20,0}, {0xFF9628,0xFF1EAA,0x00E1FF,0xE1EBFF}},
  {"Bioluminescent Topo Vortex", {"Ring count","Rotation speed","Twist","Color bias"}, {50,23,44,50}, {0x00FF2D,0x00DCD7,0x0064FF,0x001CC3}},
  {"Aizawa Viscous Ink Plume", {"Viscosity bifurcation","Downflow","Vortex core","Plume thickness"}, {71,19,57,44}, {0x000000,0x6A007A,0xE100FF,0xFFFFFF}},
  {"Whitney Harmonics II", {"Canon speed","Voices","Orbit size","Trail length"}, {20,58,66,58}, {0x16bffd,0x51ffc5,0xffd98b,0xc748ff}},
  {"Chladni Resonance", {"Mode N","Mode M","Morph speed","Filament width"}, {30,57,26,40}, {0x26207e,0x914fff,0xff6093,0xffcd91}},
  {"Vega444 C — Orbital", {"Grid density","Lens bulge","Breathing depth","Orbit speed"}, {45,62,58,25}, {0xeb4149,0xffbb69,0x35dcaa,0x528aff}},
  {"Warped Checkerboard — Silk", {"Flow speed","Cell size","Warp strength","Fabric fold"}, {26,47,65,52}, {0x5740cb,0xef62ca,0xffc09a,0x51dace}},
  {"Bioluminescent Topo Vortex II", {"Ring count","Flow speed","Vortex twist","Strand beads"}, {50,27,60,45}, {0x3eff88,0x22e6c8,0x3885ff,0x7829d6}},
  {"Ripple Cell Grid 2 — Liquid", {"Wave speed","Cell size","Ripple depth","Bubble size"}, {33,50,65,51}, {0xff3e56,0xffb647,0xfff19c,0x43d8b7}},
  {"Twisted Tunnel — Hyperspace", {"Travel speed","Portal density","Twist","Flight path"}, {25,53,64,42}, {0xff345e,0xffbe4f,0x21ccc4,0x5d48df}},
  {"Squares — Infinite", {"Travel speed","Square count","Corkscrew","Line weight"}, {28,48,46,34}, {0x29d8ff,0x745aff,0xff50bb,0xffbd59}},
  {"Whitney — Motion Graphics 1968", {"Motion speed","Line count","Phase spacing","Line width"}, {50,50,50,50}, {0x07181D,0xDCD5BF,0xA34A80,0xDCD5BF}},
  {"Whitney — Differential Columns", {"Motion speed","Orbit radius","Phase divergence","Dot size"}, {50,50,50,25}, {0xFFFFFF,0xFFFFFF,0xFFFFFF,0xFFFFFF}},
  {"Whitney — Chromatic Bloom", {"Bloom speed","Particle count","Petal folding","Counterpoint"}, {38,65,55,45}, {0xFBFAF8,0xF3EA83,0xC85D4D,0x4A64C7}},
  {"Bioluminescent Topo Vortex — Abyssal", {"Ring count","Flow speed","Vortex twist","Braiding"}, {50,23,44,50}, {0x00ff2d,0x00dcd7,0x0064ff,0x001cc3}},
  {"Warped Checkerboard — Interference", {"Motion speed","Wavelength","Distortion","Wave interference"}, {28,24,41,55}, {0xff3155,0xffc247,0x22d3c5,0x765dff}},
  {"Ripples", {"Expansion speed","Ring width","Spawn density","Decay"}, {64,24,55,24}, {0x36d5b6,0x3ec1de,0x4166ff,0x9654f5}},
  {"Luminous Loom", {"Weave speed","Ribbon count","Undulation","Ribbon width"}, {28,40,55,65}, {0xff6a65,0xffc983,0x23d8c6,0x5856de}},
  {"Ferrofluid", {"Flow speed","Droplets","Cohesion","Metallic reflection"}, {25,50,36,85}, {0x173c67,0x43d9cb,0xeaf1dd,0xb854e7}},
  {"Accretion", {"Orbital speed","Disc tilt","Event horizon","Turbulence"}, {30,34,45,55}, {0xffdf91,0xffa044,0xe3426c,0x7050d4}},
  {"Origami Flight", {"Motion speed","Facet count","Folding depth","Precession"}, {28,35,66,55}, {0xffbc85,0xe96685,0x56bdc6,0xb3b2ed}},
  {"Signal Orchard", {"Pulse speed","Trees","Branching depth","Surge"}, {32,20,40,70}, {0x22f6bc,0x3cc7e8,0x7874ff,0xe957b8}},
  {"Whitney — Resonant Orbits", {"Canon speed","Particle count","Rate divergence","Echo length"}, {26,50,75,40}, {0xf4e2b2,0xe18680,0x48c8c1,0x868ee5}},
  {"Whitney — Phase Architecture", {"Motion speed","Strand count","Torsion","Precession"}, {26,45,65,55}, {0xefe4c9,0xd782a6,0x74c9d0,0x9799ed}},
  {"Photo — Empty slot 1", {"Exposure","Contrast","Black level","Color temperature"}, {50,50,50,50}, {0xbca18d,0x404d61,0x909a9c,0xede8dc}},
  {"Photo — Empty slot 2", {"Exposure","Contrast","Black level","Color temperature"}, {50,50,50,50}, {0x365c47,0x939d8c,0xe2b79a,0x444955}},
  {"Pong Wars", {"Game speed","Tile size","Ball size","Bounce variation"}, {40,33,50,25}, {0xD9E8E3,0xD9E8E3,0x172B36,0x172B36}},
  {"Whitney — Resonant Orbits v2", {"Canon speed","Particle count","Rate divergence","Echo length"}, {26,65,75,40}, {0xf4e2b2,0xe18680,0x48c8c1,0x868ee5}},
  {"Warped Checkerboard — Acid Cathedral", {"Motion speed","Grid density","Arch distortion","Aliasing"}, {28,32,74,55}, {0xff3155,0xffc247,0x22d3c5,0x765dff}},
  {"Warped Checkerboard — Phase Slippage", {"Motion speed","Grid density","Phase warp","Aliasing"}, {23,63,56,82}, {0xff3155,0xffc247,0x22d3c5,0x765dff}},
  {"Warped Checkerboard — Liquid Circuit", {"Motion speed","Grid density","Liquid warp","Aliasing"}, {26,29,85,32}, {0xff3155,0xffc247,0x22d3c5,0x765dff}},
  {"Ripples — Classic 08", {"Drop frequency","Trail fade","Ring width","Wave speed"}, {10,20,20,25}, {0x3030FF,0x5030FF,0x7030FF,0x4030FF}},
  {"Plasma — Classic 08", {"Flow speed","Wave scale","Hue drift","Trail fade"}, {15,30,50,30}, {0xFF0000,0xFFFF00,0x00FFFF,0x8000FF}},
};

namespace {
// Xtensa has no hardware transcendental instructions. A 1024-entry signed
// lookup table gives sub-pixel angular accuracy without sin(), floor(), or
// modulo work in the per-pixel shader loops. It costs exactly 2 KB of RAM.
constexpr uint16_t FAST_SIN_SIZE = 1024;
constexpr uint16_t FAST_SIN_MASK = FAST_SIN_SIZE - 1;
constexpr float FAST_SIN_SCALE = FAST_SIN_SIZE / (2.f * PI);
int16_t fastSinTable[FAST_SIN_SIZE];
bool fastSinReady = false;
#if defined(MATRIX_PROFILE_WS2812) && defined(WS2812_MATRIX_WIDTH) && defined(WS2812_MATRIX_HEIGHT)
constexpr uint16_t GEOMETRY_PIXELS=WS2812_MATRIX_WIDTH*WS2812_MATRIX_HEIGHT;
#else
constexpr uint16_t GEOMETRY_PIXELS=64*64;
#endif
float polarRadius[GEOMETRY_PIXELS];
float polarAngle[GEOMETRY_PIXELS];
uint8_t scanNoise[GEOMETRY_PIXELS];
uint16_t geometryWidth = 0, geometryHeight = 0;
void ensureFastSinTable() {
  if (fastSinReady) return;
  for (uint16_t i=0;i<FAST_SIN_SIZE;i++)
    fastSinTable[i]=(int16_t)roundf(::sinf(i*(2.f*PI/FAST_SIN_SIZE))*32767.f);
  fastSinReady=true;
}
inline float fastSinApprox(float angle) {
  const int32_t index=(int32_t)(angle*FAST_SIN_SCALE);
  return fastSinTable[(uint32_t)index&FAST_SIN_MASK]*(1.f/32767.f);
}
inline float fastCosApprox(float angle) {
  const int32_t index=(int32_t)(angle*FAST_SIN_SCALE)+FAST_SIN_SIZE/4;
  return fastSinTable[(uint32_t)index&FAST_SIN_MASK]*(1.f/32767.f);
}
inline float fastAtan2Approx(float y, float x) {
  const float absY = fabsf(y) + 1e-10f;
  float ratio, angle;
  if (x >= 0.f) {
    ratio = (x - absY) / (x + absY);
    angle = PI * .25f;
  } else {
    ratio = (x + absY) / (absY - x);
    angle = PI * .75f;
  }
  angle += (.1963f * ratio * ratio - .9817f) * ratio;
  return y < 0.f ? -angle : angle;
}
inline float fastSqrtApprox(float value) {
  if (value <= 0.f) return 0.f;
  union { float f; uint32_t i; } estimate = {value};
  estimate.i = 0x5f375a86U - (estimate.i >> 1);
  float inverse = estimate.f;
  inverse *= 1.5f - .5f * value * inverse * inverse;
  return value * inverse;
}
inline float fastHypotApprox(float x, float y) {
  return fastSqrtApprox(x*x+y*y);
}

inline float unit(uint8_t value) { return value / 100.0f; }
inline float mapControl(uint8_t value, float lo, float hi) {
  return lo + (hi - lo) * unit(value);
}
inline float fract(float value) { return value - floorf(value); }
inline float clamp01(float value) { return constrain(value, 0.0f, 1.0f); }
float fastUnitPower(float value, float exponent) {
  if (exponent == 2.f) return value*value;
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

float fastExpNeg(float value) {
  static uint16_t table[513];
  static bool ready=false;
  if(!ready){for(int i=0;i<=512;i++)table[i]=(uint16_t)(::expf(-i*.03125f)*65535.f+.5f);ready=true;}
  if(value>=16.f)return 0.f;
  const float p=max(0.f,value)*32.f;
  const int index=(int)p;
  return (table[index]+(table[index+1]-table[index])*(p-index))*(1.f/65535.f);
}

uint16_t scale565(uint16_t color, float value) {
  value = clamp01(value);
  const uint16_t scale=(uint16_t)(value*256.f);
  const uint8_t r = (((color >> 11) & 0x1F) * scale)>>8;
  const uint8_t g = (((color >> 5) & 0x3F) * scale)>>8;
  const uint8_t b = ((color & 0x1F) * scale)>>8;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

inline uint16_t colorAt(const uint16_t* palette, float position,
                        float value = 1.0f) {
  const uint8_t index = (uint8_t)((int32_t)(position * 256.0f));
  return scale565(palette[index], value);
}

inline float hash21(float x, float y) {
  return fract(fastSinApprox(x * 12.9898f + y * 78.233f) * 43758.5453f);
}

inline uint32_t nextRandom(uint32_t& state) {
  state = state * 1664525UL + 1013904223UL;
  return state;
}

inline float randomUnit(uint32_t& state) {
  return (nextRandom(state) >> 8) / 16777216.0f;
}

float valueNoise(float x, float y) {
  const float ix = floorf(x), iy = floorf(y);
  const float fx = x - ix, fy = y - iy;
  const float ux = fx * fx * (3.0f - 2.0f * fx);
  const float uy = fy * fy * (3.0f - 2.0f * fy);
  const float a = hash21(ix, iy), b = hash21(ix + 1.0f, iy);
  const float c = hash21(ix, iy + 1.0f), d = hash21(ix + 1.0f, iy + 1.0f);
  return (a * (1.0f - ux) + b * ux) * (1.0f - uy) +
         (c * (1.0f - ux) + d * ux) * uy;
}

float fbm(float x, float y) {
  float result = 0.0f, amplitude = 0.57f;
  for (int octave = 0; octave < 3; octave++) {
    result += amplitude * valueNoise(x, y);
    x *= 2.03f; y *= 2.03f; amplitude *= 0.48f;
  }
  return result;
}
}

#include "PatternflowImports.inc"
#include "PatternRemixes.inc"
#include "WhitneyStudies.inc"
#include "AdvancedStudies.inc"
#include "NovaStudies.inc"
#include "AcidStudies.inc"
#include "PhotoStudies.inc"
#include "PongWars.inc"
#include "Classic08.inc"

void PatternflowPack::reset(uint8_t index) {
  if (index < PATTERNFLOW_PATTERN_COUNT) time_[index] = 0.0f;
  resetImportedPattern();
  resetRemixPattern();
  resetWhitneyPattern();
  resetAdvancedPattern();
  resetPongWars();
  resetClassic08();
}

void PatternflowPack::render(uint8_t index, uint16_t width, uint16_t height,
                             float deltaTime, const uint8_t p[4],
                             const uint16_t palette[256], const uint16_t rgbPalette[4096],
                             PatternflowPixelWriter writer, void* context) {
  if (index >= PATTERNFLOW_PATTERN_COUNT || !writer || width == 0 || height == 0) return;
  ensureFastSinTable();
  if (width > 64 || height > 64 || (uint32_t)width*height > GEOMETRY_PIXELS) return;
  if (geometryWidth != width || geometryHeight != height) {
    const float centerX=(width-1)*.5f, centerY=(height-1)*.5f;
    const float inverseHeight=1.f/height;
    for (uint16_t y=0;y<height;y++) for(uint16_t x=0;x<width;x++) {
      const uint16_t pixel=y*width+x;
      const float dx=(x-centerX)*inverseHeight,dy=(y-centerY)*inverseHeight;
      polarRadius[pixel]=fastHypotApprox(dx,dy);
      polarAngle[pixel]=fastAtan2Approx(dy,dx);
      scanNoise[pixel]=(uint8_t)(clamp01(fbm(x*.09f,y*.09f))*255.f);
    }
    geometryWidth=width;geometryHeight=height;
  }
  if(index >= 63) { renderClassic08(index-63,width,height,deltaTime,p,palette,writer,context); return; }
  if(index >= 60) { renderAcidPattern(index-60,width,height,deltaTime,p,palette,writer,context); return; }
  if(index == 59) { renderNovaPattern(7,width,height,deltaTime,p,palette,writer,context); return; }
  if(index == 58) { renderPongWars(width,height,deltaTime,p,palette,writer,context); return; }
  if(index >= 56) { renderPhotoPattern(index-56,width,height,p,rgbPalette,writer,context); return; }
  if(index >= 49) { renderNovaPattern(index-49,width,height,deltaTime,p,palette,writer,context); return; }
  if(index >= 46) { renderAdvancedPattern(index-46,width,height,deltaTime,p,palette,writer,context); return; }
  if(index >= 43) { renderWhitneyPattern(index-43,width,height,deltaTime,p,palette,rgbPalette,writer,context); return; }
  if(index >= 35) { renderRemixPattern(index-35,width,height,deltaTime,p,palette,writer,context); return; }
  if(index >= 17) { renderImportedPattern(index-17,width,height,deltaTime,p,palette,rgbPalette,writer,context); return; }
  float& t = time_[index];
  const float cx = (width - 1) * 0.5f, cy = (height - 1) * 0.5f;
  const float aspect = width / (float)height;
  const float inverseWidth=1.f/width,inverseHeight=1.f/height;
  const float inverseEdgeWidth=1.f/max(1,(int)width-1);
  const float inverseEdgeHeight=1.f/max(1,(int)height-1);
  auto put = [&](int x, int y, float hue, float value = 1.0f) {
    if (value > 0.005f) writer(context, x, y, colorAt(palette, hue, value));
  };
  auto putRgb=[&](int x,int y,float r,float g,float b){
    const int R=constrain((int)r,0,255),G=constrain((int)g,0,255),B=constrain((int)b,0,255);
    writer(context,x,y,rgbPalette[((R>>4)<<8)|((G>>4)<<4)|(B>>4)]);
  };
  struct Point3 { float x, y, z; };
  struct Point2 { float x, y, z; };
  auto rotate3 = [](Point3 v, float ax, float ay, float az) {
    const float sx=fastSinApprox(ax),cx=fastCosApprox(ax),sy=fastSinApprox(ay),cy=fastCosApprox(ay),sz=fastSinApprox(az),cz=fastCosApprox(az);
    const float y1=v.y*cx-v.z*sx,z1=v.y*sx+v.z*cx;
    const float x2=v.x*cy+z1*sy,z2=-v.x*sy+z1*cy;
    return Point3{x2*cz-y1*sz,x2*sz+y1*cz,z2};
  };
  auto drawLine = [&](Point2 a, Point2 b, float hueA, float hueB,
                      float value, int thickness) {
    int x0=(int)roundf(a.x),y0=(int)roundf(a.y),x1=(int)roundf(b.x),y1=(int)roundf(b.y);
    const int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1;
    int error=dx+dy,steps=max(dx,-dy),step=0;
    while(true){
      const float mix=steps>0?step/(float)steps:0.f;
      const float hue=hueA+(hueB-hueA)*mix;
      const int radius=max(0,thickness-1);
      for(int oy=-radius;oy<=radius;oy++)for(int ox=-radius;ox<=radius;ox++)
        if(abs(ox)+abs(oy)<=radius)put(x0+ox,y0+oy,hue,value);
      if(x0==x1&&y0==y1)break;
      const int twice=2*error;
      if(twice>=dy){error+=dy;x0+=sx;}
      if(twice<=dx){error+=dx;y0+=sy;}
      step++;
    }
  };
  auto ambientSpeed = [](uint8_t value, float slow, float fast) {
    float n=value/100.f;n=n*n*(3.f-2.f*n);return slow+(fast-slow)*n;
  };
  auto quickDistance = [](float x, float y) {
    x=fabsf(x);y=fabsf(y);const float hi=max(x,y),lo=min(x,y);
    return hi+lo*.375f;
  };

  switch (index) {
    case 0: { // Vector Field Flow
      const float turbulence = unit(p[0]);
      t += deltaTime * mapControl(p[1], 0.2f, 6.0f);
      const float density = mapControl(p[2], 1.0f, 20.0f);
      for (int y=0; y<height; y++) for (int x=0; x<width; x++) {
        const float nx=(x*inverseWidth-.5f)*aspect, ny=y*inverseHeight-.5f;
        const float angle=fastSinApprox(nx*density+t)+fastCosApprox(ny*density-t);
        const float vx=fastSinApprox(angle*turbulence*5), vy=fastCosApprox(angle*turbulence*5);
        const float v=.5f+.5f*fastSinApprox((nx*vx+ny*vy)*10+t*2);
        put(x,y,unit(p[3])+angle*.16f+vx*.2f,v);
      } break;
    }
    case 1: { // Chromatic Vortex
      t += deltaTime * mapControl(p[1],.2f,5.f);
      const float split=unit(p[0])*2.5f, density=mapControl(p[2],3.f,18.f);
      for(int y=0;y<height;y++) for(int x=0;x<width;x++) {
        const float r=polarRadius[y*width+x],a=polarAngle[y*width+x];
        const float bands=.5f+.5f*fastSinApprox(r*density-t*2+a+split*fastSinApprox(r*9-t));
        put(x,y,a/(2*PI)+r+unit(p[3])*.45f,bands);
      } break;
    }
    case 2: { // Liquid Ripple
      t += deltaTime*mapControl(p[1],.1f,4.f);
      const float refract=mapControl(p[0],0,4),waves=mapControl(p[2],4,24),spec=unit(p[3]);
      for(int y=0;y<height;y++) for(int x=0;x<width;x++) {
        const float dx=(x-cx)*inverseHeight,dy=(y-cy)/(float)height,r=fastSqrtApprox(dx*dx+dy*dy);
        const float ripple=fastSinApprox(r*waves-t*3+fastSinApprox((dx-dy)*7+t)*refract);
        put(x,y,.52f+ripple*.12f+t*.01f,clamp01(.35f+.45f*ripple+spec*fastUnitPower(fabsf(ripple),8)));
      } break;
    }
    case 3: { // Kaleidoscope, distinct from the existing point pattern
      t += deltaTime*mapControl(p[1],.1f,4.f);
      const int symmetry=3+(int)(unit(p[0])*9); const float twist=mapControl(p[3],0,5),depth=mapControl(p[2],2,18);
      for(int y=0;y<height;y++) for(int x=0;x<width;x++) {
        const float r=polarRadius[y*width+x],a=polarAngle[y*width+x];
        const float folded=fabsf(fract((a/(2*PI)+.5f)*symmetry)*2-1);
        const float v=.5f+.5f*fastSinApprox(r*depth-folded*PI*2+t*2+twist*r);
        put(x,y,folded+r*.4f+t*.015f,v);
      } break;
    }
    case 4: { // Isometric Pillars
      t += deltaTime*mapControl(p[1],.1f,4.f)*1.5f;
      const int cell=max(2,6-(int)(unit(p[0])*4)); const float maxHeight=mapControl(p[2],1,8),shift=unit(p[3]);
      for(int y=0;y<height;y++) for(int x=0;x<width;x++) {
        const int gx=(x+y)/cell,gy=(y-x+width)/cell; const float h=(.5f+.5f*fastSinApprox(fastHypotApprox(gx-width/cell,gy-height/cell)*.8f-t))*maxHeight;
        const int local=(x+y)%cell; const float face=local<cell/3?1.f:(local<2*cell/3?.65f:.38f);
        put(x,y,shift+h*.04f,face*(.25f+h/maxHeight*.75f));
      } break;
    }
    case 5: { // Color Ribbons
      t += deltaTime*mapControl(p[1],.1f,4.f)*2;
      const int ribbons=2+(int)(unit(p[2])*8); const float amp=mapControl(p[3],2,height*.45f);
      float ribbonY[64][10];
      for(int x=0;x<width;x++)for(int i=0;i<ribbons;i++)ribbonY[x][i]=cy+(fastSinApprox(x*.18f+t+i*1.337f)+fastCosApprox(x*.27f-t*.8f+i*1.337f))*amp;
      for(int y=0;y<height;y++) for(int x=0;x<width;x++) {float glow=0,hue=0;for(int i=0;i<ribbons;i++){const float d=fabsf(y-ribbonY[x][i]);if(d<4){const float g=fastUnitPower(1/(d+1),1.5f);glow+=g;hue+=unit(p[0])+i*.1f;}}if(glow>.1f)put(x,y,hue/max(1,ribbons),clamp01(glow*.8f));} break;
    }
    case 6: { // Folded Cross
      t += deltaTime*mapControl(p[1],.1f,4.f);
      const int fold=4+(int)(unit(p[0])*10); const float limit=mapControl(p[2],0,4.9f),mask=unit(p[3]);
      for(int y=0;y<height;y++) for(int x=0;x<width;x++){const float fx=abs(x%(fold*2)-fold)*limit,fy=abs(y%(fold*2)-fold)*limit,c=fabsf(fastSinApprox(fx*.2f+t)*fastCosApprox(fy*.2f-t)+fastCosApprox(fx*.1f-t*1.5f)*fastSinApprox(fy*.1f+t));if(c>.65f)put(x,y,mask<.5f?.9f:.48f,clamp01((c-.65f)/.35f));else if(c<.08f)put(x,y,.16f,.65f);} break;
    }
    case 7: { // Bitwise Shards
      t += deltaTime*mapControl(p[1],.1f,4.f);
      const int block=2+(int)(unit(p[0])*6),shift=(int)mapControl(p[2],1,5); const float base=unit(p[3]);
      for(int y=0;y<height;y++) for(int x=0;x<width;x++){const int bx=y/block,ax=x/block,mx=abs(x%(block*2)-block),my=abs(y%(block*2)-block),id=(ax^bx)<<shift;const float v=fabsf(fastSinApprox((mx+my)*.3f+id*.1f+t));if(v>.6f)put(x,y,base+(ax^bx)*.07f,(v-.6f)/.4f);else if(v<.05f)put(x,y,base+.5f,.15f);} break;
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
        cellX[i] = fastSinApprox(t*orbitA[i]+orbitPhase[i])*orbitRadius[i]*aspect;
        cellY[i] = fastCosApprox(t*orbitB[i]+orbitPhase[i]+1.f)*orbitRadius[i];
        pulse[i] = .88f+.12f*fastSinApprox(t*2.1f+orbitPhase[i]*3.7f);
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
        const float f1=fastSqrtApprox(f1sq), border=max(0.f,fastSqrtApprox(f2sq)-f1);
        const float radius=fastSqrtApprox(nx*nx+ny*ny);
        const float plasma=(fastSinApprox(nx*7+t*1.3f)+fastSinApprox(ny*6-t)+
          fastSinApprox((nx+ny)*5+t*.8f)+fastSinApprox(radius*9-t*1.7f))*.04f*plasmaStrength;
        const float distance=max(0.f,f1+plasma);
        const float glow=softExpNeg(distance*4.f),core=softExpNeg(distance*14.f);
        const float rim=softExpNeg(border*9.f)*.7f;
        const float brightness=clamp01(glow*1.1f+rim+core*.45f);
        const float hue=baseHue+nearest/(float)cells+border*.3f+plasma*.12f+t*.03f;
        if(brightness>.012f)putRgb(x,y,(.5f+.5f*fastCosApprox(hue*2*PI))*brightness*255,
          (.5f+.5f*fastCosApprox((hue*1.1f+.33f)*2*PI))*brightness*255,
          (.5f+.5f*fastCosApprox((hue*.9f+.67f)*2*PI))*brightness*255);
      } break;
    }
    case 9: { // Chladni Acoustic Waves by @engmung
      const float n=mapControl(p[0],1,8),m=mapControl(p[1],1,12);
      t += deltaTime*mapControl(p[2],.2f,2.f);
      const float sharp=mapControl(p[3],1,20);
      const float nMod=n+fastSinApprox(t)*.5f,mMod=m+fastCosApprox(t*.8f)*.5f;
      for(int y=0;y<height;y++)for(int x=0;x<width;x++){
        const float nx=(x*inverseEdgeWidth-.5f)*2.f;
        const float ny=(y*inverseEdgeHeight-.5f)*2.f;
        const float field=fastCosApprox(nMod*PI*nx)*fastCosApprox(mMod*PI*ny)-
                          fastCosApprox(mMod*PI*nx)*fastCosApprox(nMod*PI*ny);
        const float node=softExpNeg(fabsf(field)*sharp);
        if(node>.002f){float low=clamp01(node/.49f),high=clamp01((node-.49f)/.51f);
          putRgb(x,y,102*low+153*high,0,255*low*(1-high));}
      } break;
    }
    case 10: { // Pulsating Solar Flare by @martinnlopez
      const int count=constrain((int)roundf(mapControl(p[0],50,400)),50,400);
      t += deltaTime*mapControl(p[1],.1f,10.f);
      const float frequency=mapControl(p[2],1,10),glow=unit(p[3]);
      const float sizeScale=min(width,height)/64.f;
      const float goldenAngle=2.39996f;
      for(int i=1;i<count;i++){
        const float baseRadius=3.f*fastSqrtApprox(i)*sizeScale;
        const float pulse=fastSinApprox(baseRadius/max(.1f,sizeScale)*.2f-t)*frequency*sizeScale;
        const float radius=baseRadius+pulse,angle=i*goldenAngle+t*.2f;
        const int x=(int)floorf(cx+fastCosApprox(angle)*radius),y=(int)floorf(cy+fastSinApprox(angle)*radius);
        if(x<0||x>=width||y<0||y>=height)continue;
        const float value=clamp01(.58f+.42f*fastSinApprox(pulse/max(.1f,sizeScale)*.5f));
        const float hue=.96f+radius/max(1.f,(float)min(width,height))*.16f+t*.008f;
        putRgb(x,y,value*255,value*glow*255,0);
        if(glow>.05f){
          const float bloom=value*glow*.5f;
          if(x+1<width)putRgb(x+1,y,bloom*255,0,0);
          if(y+1<height)putRgb(x,y+1,bloom*255,0,0);
          if(glow>.65f&&x>0)put(x-1,y,hue+.02f,bloom*.6f);
        }
      } break;
    }
    case 11: { // PsiOp point-cloud Death Star
      t += deltaTime*mapControl(p[0],.08f,1.6f);
      const int latitudes=constrain((int)roundf(mapControl(p[1],10.f,28.f)),10,28);
      const int longitudes=constrain((int)roundf(mapControl(p[1],16.f,44.f)),16,44);
      const float dishRadius=mapControl(p[2],.18f,.52f);
      const float equatorGlow=mapControl(p[3],.25f,1.f);
      const float radius=min(width,height)*.39f;
      const float az=-t*.72f,tilt=.22f+.12f*fastSinApprox(t*.23f);
      auto projectStar=[&](Point3 v){
        const Point3 q=rotate3(v,tilt,0,az);
        return Point2{cx+q.x*radius,cy+q.y*radius,q.z};
      };
      for(int j=1;j<latitudes;j++){
        const float latitude=-PI*.5f+PI*j/latitudes;
        const float cl=fastCosApprox(latitude),sl=fastSinApprox(latitude);
        for(int i=0;i<longitudes;i++){
          const float longitude=2*PI*i/longitudes;
          const Point3 local={cl*fastCosApprox(longitude),cl*fastSinApprox(longitude),sl};
          const bool insideDish=local.x>.65f &&
            local.y*local.y+local.z*local.z<dishRadius*dishRadius;
          if(!insideDish){
            const Point2 q=projectStar(local);
            put((int)roundf(q.x),(int)roundf(q.y),.58f+.24f*q.z+t*.012f,
                .48f+.48f*(q.z*.5f+.5f));
          }
        }
      }
      Point2 previous={},first={};
      for(int i=0;i<=longitudes;i++){
        const float a=2*PI*(i%longitudes)/longitudes;
        const Point2 q=projectStar({fastCosApprox(a),fastSinApprox(a),0});
        if(i==0)first=q;else drawLine(previous,q,.47f,.68f,equatorGlow,1);
        previous=q;
      }
      const int dishSegments=max(12,longitudes/2);
      for(int ring=0;ring<2;ring++){
        const float rr=dishRadius*(ring? .66f:1.f);
        for(int i=0;i<=dishSegments;i++){
          const float a=2*PI*(i%dishSegments)/dishSegments;
          const float x=fastSqrtApprox(max(0.f,1.f-rr*rr));
          const Point2 q=projectStar({x,rr*fastCosApprox(a),rr*fastSinApprox(a)});
          if(i)drawLine(previous,q,.02f+.15f*ring,.12f+.15f*ring,.95f,1);
          else first=q;
          previous=q;
        }
      }
      break;
    }
    case 12: { // Pendulum Waves
      t+=deltaTime*ambientSpeed(p[0],.22f,4.2f);
      const float base=mapControl(p[1],.75f,5.5f);
      const float spread=mapControl(p[2],.05f,6.f);
      const float margin=mapControl(p[3],0.f,min(width,height)*.32f);
      const float amplitude=max(.2f,cy-margin);
      Point2 previous={0,0,0};
      for(int x=0;x<width;x++){
        const float k=width>1?x/(float)(width-1):0;
        const float y=cy+amplitude*fastSinApprox((base+k*spread)*t);
        const Point2 q={(float)x,y,0};
        if(x)drawLine(previous,q,k*.45f+t*.012f,k*.45f+t*.012f,.78f,1);
        put(x,(int)roundf(y),k*.45f+t*.012f,1.f);
        previous=q;
      }break;
    }
    case 13: { // Nachtschwärmer metaballs
      t+=deltaTime*ambientSpeed(p[0],.16f,3.2f);
      const float radius=mapControl(p[1],1.f,4.8f);
      const float threshold=mapControl(p[2],5.2f,1.25f);
      const int balls=constrain((int)roundf(mapControl(p[3],1.f,8.f)),1,8);
      float bx[8],by[8];
      for(int i=0;i<balls;i++){
        bx[i]=cx+fastSinApprox(t*(.71f+i*.073f)+i*2.17f)*width*(.18f+.018f*i);
        by[i]=cy+fastCosApprox(t*(.63f+i*.061f)+i*1.43f)*height*(.17f+.016f*i);
      }
      for(int y=0;y<height;y++)for(int x=0;x<width;x++){
        float field=0;
        for(int i=0;i<balls;i++)field+=radius/max(.22f,quickDistance(x-bx[i],y-by[i]));
        if(field>threshold){const float v=clamp01((field-threshold)*.72f+.12f);
          put(x,y,.075f+field*.025f+t*.008f,v);}
      }break;
    }
    case 14: { // Cathedral Glass
      t+=deltaTime*ambientSpeed(p[0],.14f,2.2f);
      const float cellScale=mapControl(p[1],.6f,1.25f);
      const float lead=mapControl(p[2],.18f,.95f);
      const float spread=mapControl(p[3],.03f,.28f);
      constexpr int seeds=7;float sx[seeds],sy[seeds];
      for(int i=0;i<seeds;i++){
        sx[i]=cx+fastSinApprox(t*(1.1f+i*.07f)+i*2.11f)*(width*.32f*cellScale)+fastCosApprox(i*1.7f)*2;
        sy[i]=cy+fastCosApprox(t*(.9f+i*.05f)+i*1.73f)*(height*.32f*cellScale)+fastSinApprox(i*2.3f)*2;
      }
      for(int y=0;y<height;y++)for(int x=0;x<width;x++){
        float best=1e9f,second=1e9f;int nearest=0;
        for(int i=0;i<seeds;i++){const float dx=x-sx[i],dy=y-sy[i],d=dx*dx+dy*dy;
          if(d<best){second=best;best=d;nearest=i;}else if(d<second)second=d;}
        const float edge=clamp01((second-best)*lead*.035f);
        const float pane=.55f+.45f*fastSinApprox(best*.075f+t*1.4f+nearest);
        put(x,y,nearest*spread+pane*.025f+t*.006f,
            clamp01(.14f+pane*.38f+(1.f-edge)*.46f));
      }break;
    }
    case 15: { // Tidal Topography
      t+=deltaTime*ambientSpeed(p[0],.16f,2.6f);
      const float spacing=mapControl(p[1],.38f,1.05f);
      const float depth=mapControl(p[2],.2f,.95f);
      const float weight=mapControl(p[3],1.9f,1.35f);
      for(int y=0;y<height;y++)for(int x=0;x<width;x++){
        const float r=quickDistance(x-cx,y-cy);
        float field=r*spacing+fastSinApprox(x*.42f+t*3.1f)*depth+
          fastSinApprox(y*.38f-t*2.2f)*depth*.75f+fastSinApprox((x+y)*.19f+t*1.3f)*depth*.55f;
        const float line=1.f-clamp01(fabsf(fastSinApprox(field))*weight);
        const float water=.5f+.5f*fastSinApprox(field*.31f-t*1.7f);
        put(x,y,.48f+water*.08f+r*.006f,clamp01(.1f+water*.28f+line*.57f));
      }break;
    }
    case 16: { // Whitney Harmonics
      t+=deltaTime*ambientSpeed(p[0],.16f,3.f);
      const int voices=constrain((int)roundf(mapControl(p[1],5.f,13.f)),5,13);
      const float radius=mapControl(p[2],min(width,height)*.15f,min(width,height)*.44f);
      const int ghosts=constrain((int)roundf(mapControl(p[3],1.f,4.f)),1,4);
      for(int ghost=ghosts-1;ghost>=0;ghost--){
        const float gt=t-ghost*.035f;Point2 previous={0,0,0};
        for(int i=0;i<voices;i++){
          const float k=i/(float)max(1,voices-1),phase=gt*(1.f+i*.1618f)*2*PI+i*.55f;
          const float rr=radius*(.18f+.82f*k);
          const Point2 q={cx+fastCosApprox(phase)*rr+fastSinApprox(phase*.381966f+gt*1.7f)*1.6f,
                          cy+fastSinApprox(phase*1.618034f)*rr*.72f+fastCosApprox(phase*.5f)*1.4f,0};
          put((int)roundf(q.x),(int)roundf(q.y),i/(float)voices+t*.01f,
              (1.f-ghost/(float)(ghosts+1))*.92f);
          if(i)drawLine(previous,q,(i-1)/(float)voices,i/(float)voices,
                        .24f/(ghost+1),1);previous=q;
        }
      }break;
    }
    default: break;
  }
}
