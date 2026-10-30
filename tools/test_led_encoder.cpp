#include <cassert>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include "../src/platform/LedEncoder.h"

static uint16_t rgb565(int r, int g, int b) { return (uint16_t)(((r * 31 + 127) / 255) << 11 | ((g * 63 + 127) / 255) << 5 | (b * 31 + 127) / 255); }
static int red(uint32_t c) { return c >> 16; }
static int green(uint32_t c) { return (c >> 8) & 255; }
static int blue(uint32_t c) { return c & 255; }
static double linear(double encoded) { return encoded <= .04045 ? encoded / 12.92 : std::pow((encoded + .055) / 1.055, 2.4); }

// Mean output over one 4×4 dither tile.
static void mean(const LedEncoder& e, uint16_t color, uint32_t gain, double out[3]) {
  out[0] = out[1] = out[2] = 0;
  for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) {
    const uint32_t c = e.encode(color, x, y, gain);
    out[0] += red(c) / 16.0; out[1] += green(c) / 16.0; out[2] += blue(c) / 16.0;
  }
}

// Every LED's channels hold the colour's proportions to its brightest channel,
// within half a level, scaled by the balance in percent.
static void assertProportional(const LedEncoder& e, uint16_t color, uint32_t gain, const int balance[3]) {
  const double source[3] = {linear((color >> 11) / 31.0) * balance[0], linear(((color >> 5) & 63) / 63.0) * balance[1],
                            linear((color & 31) / 31.0) * balance[2]};
  const double peak = std::fmax(source[0], std::fmax(source[1], source[2]));
  for (int i = 0; i < 16; i++) {
    const uint32_t c = e.encode(color, i & 3, i >> 2, gain);
    const int out[3] = {red(c), green(c), blue(c)};
    const int top = out[0] > out[1] ? (out[0] > out[2] ? out[0] : out[2]) : (out[1] > out[2] ? out[1] : out[2]);
    for (int k = 0; k < 3; k++) assert(std::fabs(out[k] - source[k] / peak * top) <= .5 + 1e-3 * top);
  }
}

int main() {
  LedEncoder e;
  e.begin();
  const int neutral[3] = {100, 100, 100};

  assert(LedEncoder::gain(0, 1) == 0 && LedEncoder::gain(15, 1) == 65536);
  for (int b = 1; b <= 15; b++) assert(LedEncoder::gain(b, 1) > LedEncoder::gain(b - 1, 1));
  assert(LedEncoder::gain(15, .5f) == 32768 && LedEncoder::gain(15, 2) == 65536 && LedEncoder::gain(15, -1) == 0);

  // Black stays black at every brightness and dither position.
  for (int b = 0; b <= 15; b++) for (int i = 0; i < 16; i++) assert(e.encode(0, i & 3, i >> 2, LedEncoder::gain(b, 1)) == 0);
  // Full white at full brightness is exactly 255 everywhere.
  for (int i = 0; i < 16; i++) assert(e.encode(0xFFFF, i & 3, i >> 2, LedEncoder::gain(15, 1)) == 0xFFFFFF);

  // Each LED keeps its colour's hue at every brightness: warm, pastel, deep and
  // neutral colours, including Bonfire's stops, never light a channel out of proportion.
  for (int b = 1; b <= 15; b++)
    for (uint32_t rgb : {0x790002u, 0xE53F00u, 0xFFE05Eu, 0xF58800u, 0xE89A3Cu, 0xFED1D3u, 0x2E663Cu, 0x1E59CDu, 0x808080u, 0x202020u})
      for (int v = 255; v >= 8; v -= 31)
        assertProportional(e, rgb565(((rgb >> 16) & 255) * v / 255, ((rgb >> 8) & 255) * v / 255, (rgb & 255) * v / 255),
                           LedEncoder::gain(b, 1), neutral);

  // An orange body at the lowest brightness lights red alone, never a yellow LED among red ones.
  for (int i = 0; i < 16; i++) assert(green(e.encode(rgb565(0xE5, 0x3F, 0), i & 3, i >> 2, LedEncoder::gain(1, 1))) == 0);

  // The brightest channel's dithered mean follows the target within a quarter step, so dim gradients keep detail.
  for (int b : {2, 3, 5}) for (int v = 0; v <= 255; v += 5) {
    const uint16_t c = rgb565(v, 0, 0);
    double m[3]; mean(e, c, LedEncoder::gain(b, 1), m);
    assert(std::fabs(m[0] - linear((c >> 11) / 31.0) * (b / 15.0) * (b / 15.0) * 255) <= 0.26);
  }

  // Brightness 3 drives white near 4% of full power.
  double white[3]; mean(e, 0xFFFF, LedEncoder::gain(3, 1), white);
  assert(white[0] > 9 && white[0] < 11.5);

  // The white balance scales each channel in linear light and keeps proportions in every LED.
  const int warm[3] = {100, 70, 85};
  e.setBalance(warm[0], warm[1], warm[2]);
  for (int i = 0; i < 16; i++) {
    const uint32_t c = e.encode(0xFFFF, i & 3, i >> 2, LedEncoder::gain(15, 1));
    assert(red(c) == 255 && std::abs(green(c) - 179) <= 1 && std::abs(blue(c) - 217) <= 1);
  }
  for (int b = 1; b <= 15; b++) for (int v = 255; v >= 8; v -= 31) assertProportional(e, rgb565(v, v, v), LedEncoder::gain(b, 1), warm);
  // Brightness 1 drives three steps, so yellow stays yellow in every LED under the WS2812 balance.
  e.setBalance(100, 69, 94);
  assert(LedEncoder::gain(1, 1) == 771);
  for (int i = 0; i < 16; i++) {
    const uint32_t c = e.encode(0xFFE0, i & 3, i >> 2, LedEncoder::gain(1, 1));
    assert(red(c) >= 2 && green(c) >= 1 && blue(c) == 0);
  }
  e.setBalance(100, 100, 100);
  assert(e.encode(0xFFFF, 0, 0, LedEncoder::gain(15, 1)) == 0xFFFFFF);
  return 0;
}
