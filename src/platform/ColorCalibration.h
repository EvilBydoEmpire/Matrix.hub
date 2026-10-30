#pragma once
#include <stdint.h>

// Scales each RGB565 channel by a percentage, rounding to nearest.
inline uint16_t channelBalanced565(uint16_t color, uint8_t red,
                                   uint8_t green, uint8_t blue) {
  return (((((color >> 11) & 31) * red + 50) / 100) << 11) |
         (((((color >> 5) & 63) * green + 50) / 100) << 5) |
         (((color & 31) * blue + 50) / 100);
}
