#pragma once
#include <math.h>
#include <stdint.h>

// Xtensa has no hardware transcendental instructions, so per-pixel shaders use
// a 1024-entry sine table in internal RAM and cheap, bounded approximations.
constexpr uint16_t FAST_SIN_SIZE = 1024;
constexpr uint16_t FAST_SIN_MASK = FAST_SIN_SIZE - 1;
constexpr float FAST_TAU = 6.28318530718f;
constexpr float FAST_SIN_SCALE = FAST_SIN_SIZE / FAST_TAU;
extern int16_t fastSinTable[FAST_SIN_SIZE];
// Fills the sine table once; call it before drawing.
void ensureFastMath();

// The ESP32 runs code from flash through a 32 KB two-way cache, so code that a
// pattern calls for every pixel either inlines into the pattern (PIXEL_INLINE)
// or runs from internal RAM (IRAM_ATTR). A pattern's pixel loop then shares the
// cache only with its own code and data, wherever the linker places them.
#define PIXEL_INLINE __attribute__((always_inline))
#if defined(ESP_PLATFORM)
#include <esp_attr.h>
#else
#define IRAM_ATTR
#endif

PIXEL_INLINE inline int floorInt(float value) { const int n = (int)value; return n - (value < n); }
// The nearest integer, halves away from zero as roundf rounds them.
PIXEL_INLINE inline int roundInt(float value) { return value < 0.f ? -floorInt(.5f - value) : floorInt(value + .5f); }
PIXEL_INLINE inline float clamp01(float value) { return value < 0.f ? 0.f : value > 1.f ? 1.f : value; }

PIXEL_INLINE inline float fastSin(float angle) {
  return fastSinTable[(uint32_t)(int32_t)(angle * FAST_SIN_SCALE) & FAST_SIN_MASK] * (1.f / 32767.f);
}
PIXEL_INLINE inline float fastCos(float angle) {
  return fastSinTable[(uint32_t)((int32_t)(angle * FAST_SIN_SCALE) + FAST_SIN_SIZE / 4) & FAST_SIN_MASK] * (1.f / 32767.f);
}
// Interpolated, so slow sub-pixel motion keeps moving instead of stepping.
PIXEL_INLINE inline float smoothSin(float angle) {
  const float position = angle * FAST_SIN_SCALE;
  const int index = floorInt(position);
  const float first = fastSinTable[(uint32_t)index & FAST_SIN_MASK];
  return (first + (fastSinTable[(uint32_t)(index + 1) & FAST_SIN_MASK] - first) * (position - index)) * (1.f / 32767.f);
}
PIXEL_INLINE inline float smoothCos(float angle) { return smoothSin(angle + FAST_TAU * .25f); }

PIXEL_INLINE inline float fastAtan2(float y, float x) {
  const float absY = fabsf(y) + 1e-10f;
  float ratio, angle;
  if (x >= 0.f) { ratio = (x - absY) / (x + absY); angle = FAST_TAU * .125f; }
  else { ratio = (x + absY) / (absY - x); angle = FAST_TAU * .375f; }
  angle += (.1963f * ratio * ratio - .9817f) * ratio;
  return y < 0.f ? -angle : angle;
}
PIXEL_INLINE inline float fastSqrt(float value) {
  if (value <= 0.f) return 0.f;
  union { float f; uint32_t i; } estimate = {value};
  estimate.i = 0x5f375a86U - (estimate.i >> 1);
  const float inverse = estimate.f * (1.5f - .5f * value * estimate.f * estimate.f);
  return value * inverse;
}
PIXEL_INLINE inline float fastHypot(float x, float y) { return fastSqrt(x * x + y * y); }
// Alpha-max-plus-beta-min distance, within a few percent of the true one.
PIXEL_INLINE inline float fastDistance(float dx, float dy) {
  dx = fabsf(dx); dy = fabsf(dy);
  return dx > dy ? dx + dy * .375f : dy + dx * .375f;
}
// Bit-trick reciprocal with two Newton steps, within 0.001 %; 0 for 0.
PIXEL_INLINE inline float fastReciprocal(float x) {
  if (x == 0.f) return 0.f;
  const float magnitude = fabsf(x);
  union { float f; uint32_t bits; } estimate = {magnitude};
  estimate.bits = 0x7EF311C3U - estimate.bits;
  float r = estimate.f;
  r *= 2.f - magnitude * r; r *= 2.f - magnitude * r;
  return x < 0.f ? -r : r;
}
