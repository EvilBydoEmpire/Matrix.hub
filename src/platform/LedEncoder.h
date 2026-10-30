#pragma once
#include <math.h>
#include <stdint.h>

// WS2812 output stage. Patterns render full-scale RGB565; this decodes every
// channel with the same sRGB curve, applies the white balance, brightness and
// fades as linear gains, and dithers to 8 bits.
class LedEncoder {
 public:
  void begin() { setBalance(100, 100, 100); }

  // Each channel's share of full output, in percent.
  void setBalance(uint8_t red, uint8_t green, uint8_t blue) {
    for (int i = 0; i < 32; i++) {
      red_[i] = linear(i / 31.f, red);
      blue_[i] = linear(i / 31.f, blue);
    }
    for (int i = 0; i < 64; i++) green_[i] = linear(i / 63.f, green);
  }

  // Brightness 0..15 maps to (b/15)² of full output, except that brightness 1
  // drives three of 255 steps rather than one, so a dim colour keeps a hue
  // between the primaries. Scale carries fades.
  static uint32_t gain(int brightness, float scale) {
    const float share = brightness <= 0 ? 0.f : brightness == 1 ? 3.f / 255.f
                      : brightness >= 15 ? 1.f : (brightness / 15.f) * (brightness / 15.f);
    const float clamped = scale < 0.f ? 0.f : scale > 1.f ? 1.f : scale;
    return (uint32_t)lroundf(share * clamped * 65536.f);
  }

  // Returns 0xRRGGBB. Every LED is seen on its own, so the ordered threshold
  // dithers only the brightest channel's level and the other channels follow it
  // in proportion: dim colours keep their hue in each LED, and black stays black.
  uint32_t encode(uint16_t color, int x, int y, uint32_t gain) const {
    const uint32_t r = (uint32_t(red_[color >> 11]) * gain) >> 16;
    const uint32_t g = (uint32_t(green_[(color >> 5) & 63]) * gain) >> 16;
    const uint32_t b = (uint32_t(blue_[color & 31]) * gain) >> 16;
    const uint32_t peak = r > g ? (r > b ? r : b) : (g > b ? g : b);
    if (!peak) return 0;
    uint32_t level = (peak * 255u + bayer_[((y & 3) << 2) | (x & 3)] * 4096u + 2048u) >> 16;
    if (level > 255u) level = 255u;
    // Each channel's share of level, rounded.
    const uint32_t twiceLevel = 2u * level, twicePeak = 2u * peak;
    return ((r * twiceLevel + peak) / twicePeak << 16) | ((g * twiceLevel + peak) / twicePeak << 8) |
           (b * twiceLevel + peak) / twicePeak;
  }

 private:
  static uint16_t linear(float encoded, uint8_t percent) {
    const float value = encoded <= .04045f ? encoded / 12.92f
                                           : powf((encoded + .055f) / 1.055f, 2.4f);
    return (uint16_t)lroundf(value * 65535.f * (percent > 100 ? 100 : percent) / 100.f);
  }

  // Ordered-dither thresholds. A member rather than a static constant, which
  // would sit in flash, so encoding reads only RAM.
  uint8_t bayer_[16]{0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
  uint16_t red_[32]{};
  uint16_t green_[64]{};
  uint16_t blue_[32]{};
};
