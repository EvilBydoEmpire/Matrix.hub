#pragma once

#include <stdint.h>

struct SourceColors;  // Rgb565.h

// The collection: resolution-independent renderers addressed by stable variant
// IDs. Retired IDs keep an empty row and draw nothing. Patternflow-derived
// renderers are CC-BY-SA-4.0; docs/THIRD_PARTY_NOTICES.md credits every
// adaptation.
constexpr uint8_t PATTERNFLOW_PATTERN_COUNT = 68;

// A control without a label stays at its default.
struct PatternflowMetadata {
  const char* name;
  const char* controlLabel[4];
  uint8_t defaults[4];
  uint32_t originalColors[4];
};

extern const PatternflowMetadata PATTERNFLOW_METADATA[PATTERNFLOW_PATTERN_COUNT];

using PatternflowPixelWriter = void (*)(void* context, int16_t x, int16_t y,
                                        uint16_t color);

class PatternflowPack {
 public:
  void reset(uint8_t index);
  void render(uint8_t index, uint16_t width, uint16_t height, float deltaTime,
              const uint8_t controls[4], const uint16_t palette[256],
              const SourceColors& sourceColors,
              PatternflowPixelWriter writer, void* context);

 private:
  // Clocks of the renderers in PatternflowPack.cpp itself, IDs 0–16.
  static constexpr uint8_t PACK_CLOCKS = 17;
  float time_[PACK_CLOCKS] = {};
  // Pendulum Waves' base and spread phases.
  float pendulumPhase_[2] = {};
};
