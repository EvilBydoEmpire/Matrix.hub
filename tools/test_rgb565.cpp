// RGB565 packing, scaling and the colours of source RGB.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include "../src/patterns/Rgb565.h"

int16_t fastSinTable[FAST_SIN_SIZE];
static int R(uint16_t c) { return c >> 11; }
static int G(uint16_t c) { return (c >> 5) & 63; }
static int B(uint16_t c) { return c & 31; }

int main() {
  // Green keeps its own steps from eight up and red and blue's grid below, monotonically.
  for (int i = 0, previous = 0; i <= 100000; i++) {
    const int g = G(rgb565(0, i / 100000.f, 0));
    assert(g >= previous && (g >= 8 || g % 2 == 0));
    previous = g;
  }
  // Packing rounds, keeps greys grey and reaches both ends exactly.
  assert(rgb565(0, 0, 0) == 0 && rgb565(1, 1, 1) == 0xFFFF && rgb565(2, -1, .5f) == rgb565(1, 0, .5f));
  // Greys: below eight steps green is exactly twice red; above, each channel rounds to its nearest step.
  for (int i = 0; i <= 1000000; i++) {
    const float v = i / 1000000.f;
    const uint16_t c = rgb565(v, v, v);
    assert(R(c) == B(c) && (G(c) < 8 ? G(c) == 2 * R(c) : abs(G(c) - 2 * R(c)) <= 2));
  }
  // Scaling never brightens a channel or lights a dark one, keeps every grey grey
  // below eight green steps, and never leaves green alone lit from a colour whose
  // red or blue holds at least green's share.
  for (uint32_t c = 0; c < 65536; c++) for (int k = 0; k <= 256; k++) {
    const uint16_t s = scale565(c, k / 256.f);
    assert(R(s) <= R(c) && G(s) <= G(c) && B(s) <= B(c));
    if (k < 256 && R(c) == B(c) && G(c) >> 1 == R(c) && G(s) < 8) assert(G(s) == 2 * R(s) && B(s) == R(s));
    if ((R(c) || B(c)) && G(c) <= 2 * (R(c) > B(c) ? R(c) : B(c)) + 1) assert(!(G(s) && !R(s) && !B(s)));
  }
  assert(scale565(0xFFFF, 1) == 0xFFFF && scale565(0xFFFF, 0) == 0);
  puts("PASS RGB565 rounding: monotonic green, neutral greys, no lone green when dimming");

  // Original keeps source RGB, scaled by the brightness share, and Saturation moves it toward grey or full colour.
  uint16_t palette[256];
  for (int i = 0; i < 256; i++) palette[i] = rgb565(i / 255.f, 1 - i / 255.f, .5f);
  SourceColors original{palette, 1.f, 1.f, true};
  for (int r = 0; r < 256; r += 15) for (int g = 0; g < 256; g += 17) for (int b = 0; b < 256; b += 51)
    assert(sourceColor(original, r, g, b) == rgb565(r / 255.f, g / 255.f, b / 255.f));
  assert(sourceColor(original, 0x8040C0) == sourceColor(original, 0x80, 0x40, 0xC0) && sourceColor(original, -5, 300, 0) == rgb565(0, 1, 0));
  original.saturation = 0;
  assert(sourceColor(original, 200, 100, 0) == rgb565(200 / 255.f, 200 / 255.f, 200 / 255.f));
  original.saturation = 2;
  assert(sourceColor(original, 200, 150, 100) == rgb565(200 / 255.f, 100 / 255.f, 0) && sourceColor(original, 200, 190, 180) == rgb565(200 / 255.f, 180 / 255.f, 160 / 255.f));
  original = {palette, 1.f, .5f, true};
  assert(sourceColor(original, 255, 255, 255) == rgb565(.5f, .5f, .5f));

  // Any other palette takes its entry at the source hue, at the source's value, at full precision.
  const SourceColors remap{palette, 1.f, 1.f, false};
  assert(sourceColor(remap, 0, 0, 0) == 0);
  assert(sourceColor(remap, 255, 0, 0) == palette[0] && sourceColor(remap, 0, 255, 0) == palette[85] && sourceColor(remap, 0, 0, 255) == palette[170]);
  for (int v = 1; v < 256; v++) {
    assert(sourceColor(remap, v, 0, 0) == scale565(palette[0], v / 255.f));
    assert(sourceColor(remap, v, v, v) == scale565(palette[(uint8_t)(int)(v * (.749f / 255.f) * 256.f)], v / 255.f));
  }
  // Near-greys follow the grey ramp rather than a hue their rounding happens to give.
  assert(sourceColor(remap, 195, 205, 200) == scale565(palette[(uint8_t)(int)(205 * (.749f / 255.f) * 256.f)], 205 / 255.f));
  // Dim colours keep their hue: an orange at 40 is not the red at 40.
  assert(sourceColor(remap, 40, 20, 0) == scale565(palette[21], 40 / 255.f) && sourceColor(remap, 40, 20, 0) != sourceColor(remap, 40, 0, 0));
  puts("PASS source colours: Original as drawn with Saturation and brightness; other palettes by hue at full value precision");
  return 0;
}
