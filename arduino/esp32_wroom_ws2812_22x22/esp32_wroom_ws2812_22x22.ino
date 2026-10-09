// Arduino IDE entry point for an ESP32-WROOM driving a 22x22 WS2812 matrix.
//
// Wiring defaults inherited from LED_Matrix_22x22_11_patternflow.ino:
//   data: GPIO 22
//   pixel order: GRB, 800 kHz
//   layout: top-left origin, horizontal zigzag rows
//
// Keep this folder inside the esp32-matrix-platform checkout: the sketch
// deliberately includes the shared firmware and pattern engine rather than
// maintaining a second copy that could drift out of sync.

#define MATRIX_PROFILE_WS2812 1
#define WS2812_MATRIX_WIDTH 22
#define WS2812_MATRIX_HEIGHT 22
#define WS2812_DATA_PIN 22
#define WS2812_ROTATION 0
#define MATRIX_ENABLE_OTA 1

#include "../../64x64_matrix_01.ino"

// Arduino IDE only compiles source files in the sketch folder. Pull the shared
// implementations into this translation unit when building this wrapper.
#include "../../src/platform/Ws2812Backend.cpp"
#include "../../src/patterns/LuaPatternRuntime.cpp"
#include "../../src/patterns/PatternflowPack.cpp"
