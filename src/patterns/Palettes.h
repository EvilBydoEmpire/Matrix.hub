#pragma once
#include <stdint.h>
#include "PatternflowPack.h"

// Named palettes ordered shadow → body → highlight → accent, so cyclic sampling
// wraps smoothly and two-role patterns read stops 1/3 against 2/4. The colour
// palettes share one OKLCH lightness ladder; each single tone keeps one hue and
// steps it down in linear light from the pure tone, its highlight. Settings store
// these IDs: never renumber them.
namespace Palettes {
constexpr int Original = 7;  // The pattern's own authored colours.
constexpr int Count = 40;    // IDs 11–20 are retired and stay reserved.

// The app reads names, stops and order from this file (tools/build_web.py).
constexpr const char* Names[Count] = {
  "Ember", "Bonfire", "Saffron", "Terracotta", "Rosewood", "Dusk",
  "Moss", "Original", "Patina", "Ink", "Driftwood",
  nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr,
  "Peony", "Citrus", "Forest", "Sage", "Aurora", "Lagoon",
  "Glacier", "Cobalt", "Heather", "Orchid", "Neon", "Silver",
  "Red", "Orange", "Green", "Cyan", "Blue", "Purple", "Magenta",
};

constexpr uint32_t Stops[Count][4] = {
  {0x4C110A, 0xA63C0C, 0xF1AF57, 0xD76300},  // 0 Ember
  {0x790002, 0xE53F00, 0xFFE05E, 0xF58800},  // 1 Bonfire
  {0x4A2E10, 0xA5720F, 0xF4CD4B, 0xDF911A},  // 2 Saffron
  {0x4A2010, 0xAE5937, 0xECC19C, 0xC97C54},  // 3 Terracotta
  {0x4B0F1F, 0x9C3B48, 0xF5BAAE, 0xC57576},  // 4 Rosewood
  {0x391A42, 0x844A6C, 0xF5B483, 0xDE715D},  // 5 Dusk
  {0x272F11, 0x646C2A, 0xD0CF8A, 0x7E9B4D},  // 6 Moss
  {0, 0, 0, 0},                              // 7 Original: per-pattern stops
  {0x402712, 0xB56535, 0xEFC193, 0x5CA692},  // 8 Patina
  {0x142139, 0x3D5A7F, 0xEDE0C4, 0xC2673B},  // 9 Ink
  {0x2E2821, 0x706457, 0xE4DAC5, 0xA69987},  // 10 Driftwood
  {}, {}, {}, {}, {}, {}, {}, {}, {}, {},    // 11–20 retired
  {0x4E0D2E, 0xBD3E74, 0xFED1D3, 0xEB8182},  // 21 Peony
  {0x273A0F, 0x81A904, 0xFAF392, 0xF19B37},  // 22 Citrus
  {0x092917, 0x2E663C, 0xCBDA8E, 0xBD8B31},  // 23 Forest
  {0x1D3028, 0x5D7E68, 0xDDE5D0, 0x7CAE9C},  // 24 Sage
  {0x211C48, 0x049863, 0xBFF4C9, 0xA878DB},  // 25 Aurora
  {0x002C37, 0x0A7E81, 0xB8EFDF, 0x33BDA5},  // 26 Lagoon
  {0x172D43, 0x477A9F, 0xDEF3FB, 0x84BED8},  // 27 Glacier
  {0x09194E, 0x1E59CD, 0xC5E6FF, 0x53A9ED},  // 28 Cobalt
  {0x311E44, 0x7C5792, 0xE9D1F1, 0xC786B4},  // 29 Heather
  {0x3F0F49, 0xA23BAA, 0xFACEF4, 0xE574B8},  // 30 Orchid
  {0x2C085A, 0xCA13B3, 0x82FCFB, 0x07BFDE},  // 31 Neon
  {0x27292C, 0x6C6F73, 0xF0F2F4, 0xAAAEB4},  // 32 Silver
  {0x550000, 0xA40000, 0xFF0000, 0xD10000},  // 33 Red
  {0x551600, 0xA43400, 0xFF5500, 0xD14400},  // 34 Orange
  {0x005507, 0x00A417, 0x00FF2A, 0x00D120},  // 35 Green
  {0x005055, 0x009AA4, 0x00F0FF, 0x00C4D1},  // 36 Cyan
  {0x000655, 0x0016A4, 0x0028FF, 0x001FD1},  // 37 Blue
  {0x240055, 0x4C00A4, 0x7A00FF, 0x6300D1},  // 38 Purple
  {0x55002C, 0xA4005B, 0xFF0090, 0xD10075},  // 39 Magenta
};

// The app lists the colour palettes around the colour wheel, then the single
// tones, Original last.
constexpr uint8_t Order[] = {4, 21, 1, 0, 3, 8, 2, 22, 6, 23, 24, 25, 26, 27, 28, 9, 29, 30, 31, 5, 10, 32,
                             33, 34, 35, 36, 37, 38, 39, 7};

inline bool offered(int id) { return id >= 0 && id < Count && Names[id] != nullptr; }

// Default palette per native engine; retired engines hold 0.
constexpr uint8_t Native[16] = {9, 3, 0, 10, 0, 0, 31, 32, 0, 6, 0, 22, 4, 0, 26, 0};

// Default palette per stable collection variant; retired variants hold 0.
constexpr uint8_t Variants[PATTERNFLOW_PATTERN_COUNT] = {
  8, 21, 27, 30, 23, 25, 24, 28, 25, 0,    //  0– 9
  1, 32, 2, 5, 9, 26, 4, 0, 1, 0,          // 10–19
  0, 10, 1, 27, 0, 10, 0, 0, 0, 31,        // 20–29
  0, 23, 0, 25, 0, 2, 29, 0, 5, 0,         // 30–39
  0, 0, 28, 9, 0, 2, 28, 3, 8, 24,         // 40–49
  32, 0, 21, 22, 2, 29, 0, 0, 27, 3,       // 50–59
  1, 30, 26, 0, 5, 10, 8, 2,               // 60–67
};

inline uint8_t forPattern(int engine, int variant) {
  if (engine == 16) return variant >= 0 && variant < PATTERNFLOW_PATTERN_COUNT ? Variants[variant] : 0;
  return engine >= 0 && engine < 16 ? Native[engine] : 0;
}
}  // namespace Palettes
