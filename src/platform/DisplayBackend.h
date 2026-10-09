#pragma once

#include <stdint.h>

// Hardware-independent drawing surface used by built-in and scripted patterns.
// Colors are packed as 0xRRGGBB and converted by the concrete backend.
class DisplayBackend {
 public:
  virtual ~DisplayBackend() = default;

  virtual uint16_t width() const = 0;
  virtual uint16_t height() const = 0;
  virtual void clear(uint32_t rgb = 0) = 0;
  virtual void setPixel(int16_t x, int16_t y, uint32_t rgb) = 0;
  virtual void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                        uint32_t rgb) = 0;
  virtual void drawRect(int16_t x, int16_t y, int16_t w, int16_t h,
                        uint32_t rgb) = 0;
  virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                        uint32_t rgb) = 0;
  virtual void drawCircle(int16_t x, int16_t y, int16_t radius,
                          uint32_t rgb) = 0;
  virtual void setOutputScale(float scale) = 0;
  virtual void show() = 0;
};
