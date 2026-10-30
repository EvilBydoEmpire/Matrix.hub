#pragma once
#include <stdint.h>
#include "FastMath.h"

// Every pattern colours its pixels through these helpers: the small ones always
// inline, and sourceColor runs from internal RAM wherever it isn't inlined.

// RGB565 gives green twice the steps of red and blue, so rounding a dim colour
// channel by channel can leave green alone lit, or a grey without its green.
// Below eight green steps, green therefore rounds exactly as red and blue do.

// Packs 0..1 channels, rounding each.
PIXEL_INLINE inline uint16_t rgb565(float r, float g, float b) {
  r = clamp01(r); g = clamp01(g); b = clamp01(b);
  uint32_t green = (uint32_t)(g * 63.f + .5f);
  if (green < 8) green = (uint32_t)(g * 31.f + .5f) << 1;
  return (uint16_t)(((uint32_t)(r * 31.f + .5f) << 11) | (green << 5) | (uint32_t)(b * 31.f + .5f));
}
// Scales an RGB565 colour by 0..1, rounding each channel.
PIXEL_INLINE inline uint16_t scale565(uint16_t color, float value) {
  const uint32_t k = (uint32_t)(clamp01(value) * 256.f + .5f);
  if (k >= 256) return color;
  const uint32_t g6 = (color >> 5) & 63;
  uint32_t green = (g6 * k + 128) >> 8;
  if (green < 8) green = (((g6 >> 1) * k + 128) >> 8) << 1;
  return (uint16_t)((((((color >> 11) & 31) * k + 128) >> 8) << 11) | (green << 5) | (((color & 31) * k + 128) >> 8));
}
// A 256-entry palette table at a position around its loop, scaled by value.
PIXEL_INLINE inline uint16_t paletteColorAt(const uint16_t palette[256], float position, float value = 1.f) {
  return scale565(palette[(uint8_t)(int32_t)(position * 256.f)], value);
}
// How patterns that draw source RGB are coloured. Original keeps the source,
// Saturation aside; any other palette takes its colour at the source's hue and
// value, and near-greys run along the palette by value.
struct SourceColors {
  const uint16_t* palette;  // The pattern's palette table.
  float saturation;         // Original: 0 greys, 1 keeps, 2 boosts.
  float scale;              // Original: the share of full scale colours are drawn at.
  bool original;
};
// The colour for 0..255 channels, or for a 0xRRGGBB colour.
static inline IRAM_ATTR uint16_t sourceColor(const SourceColors& map, int r, int g, int b) {
  r = r < 0 ? 0 : r > 255 ? 255 : r; g = g < 0 ? 0 : g > 255 ? 255 : g; b = b < 0 ? 0 : b > 255 ? 255 : b;
  const int hi = r > g ? (r > b ? r : b) : (g > b ? g : b), lo = r < g ? (r < b ? r : b) : (g < b ? g : b);
  if (!hi) return 0;
  if (map.original) {
    float keep = map.saturation;
    if (keep > 1.f && hi > lo) {
      const float pure = hi * fastReciprocal(float(hi - lo));
      if (pure < keep) keep = pure;
    }
    const float unit = map.scale * (1.f / 255.f);
    return rgb565((hi - (hi - r) * keep) * unit, (hi - (hi - g) * keep) * unit, (hi - (hi - b) * keep) * unit);
  }
  float position = hi * (.749f / 255.f);
  if ((hi - lo) * 16 > hi) {
    const float inverse = fastReciprocal(float(hi - lo));
    position = (hi == r ? (g - b) * inverse : hi == g ? 2.f + (b - r) * inverse : 4.f + (r - g) * inverse) * (1.f / 6.f);
    if (position < 0.f) position += 1.f;
  }
  return paletteColorAt(map.palette, position, hi * (1.f / 255.f));
}
static inline uint16_t sourceColor(const SourceColors& map, uint32_t rgb) {
  return sourceColor(map, int((rgb >> 16) & 255), int((rgb >> 8) & 255), int(rgb & 255));
}
