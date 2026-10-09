#pragma once

#include <stdint.h>

// Native, resolution-independent adaptations of Patternflow animations. The
// Patternflow-derived algorithms remain CC-BY-SA-4.0; see THIRD_PARTY_NOTICES.md and the
// preserved JavaScript sources under patterns/imports/patternflow.
// Stable IDs include retired slots; the active catalog is separate.
constexpr uint8_t PATTERNFLOW_PATTERN_COUNT = 68;

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
              const uint16_t rgbPalette[4096],
              PatternflowPixelWriter writer, void* context);

 private:
  float time_[PATTERNFLOW_PATTERN_COUNT] = {};

};
