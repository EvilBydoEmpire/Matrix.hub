#pragma once
#include <stdint.h>
#include <math.h>

// Display transfer, NOT an ICC gamut conversion. Procedural pattern RGB is
// deliberately left alone; only photo mode opts into this tonal transform.
inline uint8_t photoGammaValue(uint8_t value, uint16_t gamma100) {
  return uint8_t(lroundf(powf(value / 255.f, gamma100 / 100.f) * 255.f));
}
inline uint16_t channelBalanced565(uint16_t color, uint8_t red,
                                   uint8_t green, uint8_t blue) {
  return (((((color >> 11) & 31) * red + 50) / 100) << 11) |
         (((((color >> 5) & 63) * green + 50) / 100) << 5) |
         (((color & 31) * blue + 50) / 100);
}
// Match Protomatter's actual MSB truncation, not idealized rounding. Used on
// a >=4-bit driver to compare the exact same image with 3-bit output.
inline uint16_t simulateThreeBit565(uint16_t color) {
  unsigned r=(color>>13)&7,g=(color>>8)&7,b=(color>>2)&7;
  return (((r*31+3)/7)<<11)|(((g*63+3)/7)<<5)|((b*31+3)/7);
}
