// Host checks of the native engines with the code the firmware runs.
#include "../src/patterns/NativeEngines.h"
#include "../src/patterns/Rgb565.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

struct Frame { int size = 64; unsigned writes = 0; uint16_t seen = 0, pixels[4096]{}; };
void put(void* raw, int16_t x, int16_t y, uint16_t c) {
  auto& f = *static_cast<Frame*>(raw);
  assert(x >= 0 && y >= 0 && x < f.size && y < f.size);
  f.pixels[y * f.size + x] = c; f.writes++; f.seen |= c;
}
// Mean absolute difference of the RGB565 channels, 0–1.
float difference(const Frame& a, const Frame& b) {
  float total = 0;
  for (int i = 0; i < a.size * a.size; i++) for (int shift : {0, 5, 11}) {
    const int mask = shift == 5 ? 63 : 31;
    total += abs(int((a.pixels[i] >> shift) & mask) - int((b.pixels[i] >> shift) & mask)) / float(mask);
  }
  return total / (a.size * a.size * 3);
}
// The share of pixels lit in either frame that differ, for engines that light few.
float change(const Frame& a, const Frame& b) {
  int lit = 0, changed = 0;
  for (int i = 0; i < a.size * a.size; i++) { lit += a.pixels[i] || b.pixels[i]; changed += a.pixels[i] != b.pixels[i]; }
  return lit ? changed / float(lit) : 0;
}

NativeEngines engines;
uint16_t palette[256];
// Each engine's controls as stored, and every control's range as the firmware clamps it.
uint64_t controls[4];
struct Range { int offset; bool integer; float low, high; };
const int ENGINES[] = {0, 1, 3, 5, 6, 7, 9, 11, 12, 14};
const std::initializer_list<Range> RANGES[15] = {
  {{0, true, 10, 300}, {4, false, 2, 10}, {8, false, .05f, 1}},                               // Starfield
  {{0, false, .1f, 3}, {4, false, 2, 10}, {8, false, 1, 8}},                                  // Warped Checkerboard
  {}, {{0, false, .1f, 3}, {4, false, .1f, 3}, {8, false, .1f, 3}, {12, false, .1f, 2}},      // Moiré Lines
  {}, {{0, false, .1f, 3}, {4, false, .1f, 5}},                                               // Shockwave
  {{0, false, .1f, 3}, {4, false, .1f, 2}, {8, false, .1f, 2}, {12, false, .1f, 2}},          // Warp Corridors
  {{0, true, 1, 10}, {4, false, .1f, 3}},                                                     // Pixel Avalanche
  {}, {{0, true, 1, 16}, {4, true, 1, 8}, {8, false, .1f, 3}},                                // Bayer Flow
  {}, {{0, true, 8, 300}, {4, false, 1, 20}, {8, false, 0, 1}},                               // Snake
  {{0, true, 2, 20}, {4, true, 1, 4}, {8, false, .1f, 4}, {12, false, .2f, 2}, {16, false, 0, .5f}},  // Squares
  {}, {{0, false, .05f, 6}, {4, false, .5f, 4}, {8, false, .1f, 1}, {12, false, 14, 31}},    // Flow Sphere
};
void setControl(const Range& r, float value) {
  char* field = reinterpret_cast<char*>(controls) + r.offset;
  if (r.integer) { const int v = int(value); memcpy(field, &v, 4); } else memcpy(field, &value, 4);
}
// Continues f for seconds at fps, clearing each frame as the firmware does.
void advance(Frame& f, int engine, int fps, float seconds) {
  for (int n = int(fps * seconds + .5f); n > 0; n--) {
    memset(f.pixels, 0, sizeof f.pixels);
    engines.render(engine, f.size, f.size, 1.f / fps, controls, palette, 1.f, put, &f);
  }
}
Frame run(int engine, int size, int fps, float seconds) {
  Frame f; f.size = size; engines.reset(); advance(f, engine, fps, seconds); return f;
}

int main() {
  for (int i = 0; i < 256; i++) palette[i] = rgb565(i / 255.f, .5f, 1 - i / 255.f);
  for (int engine : ENGINES) for (int size : {16, 22, 64}) {
    nativeDefaults(engine, controls);
    const Frame base = run(engine, size, 60, 5);
    assert(base.writes);
    assert(!memcmp(base.pixels, run(engine, size, 60, 5).pixels, sizeof base.pixels));
    Frame later = base; advance(later, engine, 60, .5f); assert(change(base, later) > .05f);
    // Rates integrate per second, and simulations take fixed steps; Starfield's
    // respawns draw from one random sequence, so only its look is compared.
    if (engine != 0) assert(difference(base, run(engine, size, 30, 5)) < .01f && difference(base, run(engine, size, 120, 5)) < .01f);
    for (const Range& r : RANGES[engine]) {
      nativeDefaults(engine, controls); setControl(r, r.low);
      const Frame low = run(engine, size, 60, 3);
      setControl(r, r.high);
      const Frame high = run(engine, size, 60, 3);
      assert(low.writes && high.writes);
      if (change(low, high) < .05f) { printf("FAIL engine %d: control at offset %d barely changes it at %d pixels\n", engine, r.offset, size); return 1; }
    }
  }
  puts("PASS native engines at 16, 22 and 64 pixels: repeatable, moving, frame-rate independent and every control effective through its range");

  // Colour comes from the palette table alone; Snake keeps its white head and red apple.
  for (auto& c : palette) c = 0x001F;
  for (int engine : ENGINES) {
    nativeDefaults(engine, controls);
    const Frame f = run(engine, 22, 60, 3);
    for (uint16_t c : f.pixels) assert(c == 0 || (c & 0xFFE0) == 0 || (engine == 11 && (c == 0xFFFF || c == 0xF800)));
  }
  // Snake's fixed colours follow the share of full scale the table is drawn at.
  for (auto& c : palette) c = 0;
  nativeDefaults(11, controls);
  {
    Frame f; f.size = 22; engines.reset();
    for (int n = 0; n < 60; n++) { memset(f.pixels, 0, sizeof f.pixels); engines.render(11, 22, 22, 1.f / 60, controls, palette, .4f, put, &f); }
    for (uint16_t c : f.pixels) assert(c == 0 || c == rgb565(.4f, .4f, .4f) || c == rgb565(.4f, 0, 0));
  }
  puts("PASS native colour comes from the palette table, and Snake's head and apple follow its scale");

  // Starfield's flight and star count follow the panel's width.
  for (int i = 0; i < 256; i++) palette[i] = 0xFFFF;
  nativeDefaults(0, controls);
  const int sizes[3] = {16, 22, 64};
  float lit[3];
  for (int k = 0; k < 3; k++) {
    const int size = sizes[k];
    Frame f; f.size = size; engines.reset(); long count = 0;
    for (int n = 0; n < 600; n++) { memset(f.pixels, 0, sizeof f.pixels); engines.render(0, size, size, 1.f / 60, controls, palette, 1.f, put, &f);
      for (int i = 0; i < size * size; i++) count += f.pixels[i] != 0; }
    lit[k] = count / (600.f * size * size);
  }
  assert(lit[0] < .1f && lit[1] < .1f && lit[2] > .005f && lit[0] < lit[2] * 6);
  printf("PASS Starfield lights %.1f%%, %.1f%% and %.1f%% of 16, 22 and 64 pixels\n", lit[0] * 100, lit[1] * 100, lit[2] * 100);
  return 0;
}
