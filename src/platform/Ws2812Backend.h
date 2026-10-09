#pragma once

#if defined(MATRIX_PROFILE_WS2812)

#include <Adafruit_NeoMatrix.h>

#include "DisplayBackend.h"

class Ws2812Backend final : public DisplayBackend {
 public:
  explicit Ws2812Backend(Adafruit_NeoMatrix& matrix);

  uint16_t width() const override;
  uint16_t height() const override;
  void clear(uint32_t rgb = 0) override;
  void setPixel(int16_t x, int16_t y, uint32_t rgb) override;
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                uint32_t rgb) override;
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h,
                uint32_t rgb) override;
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                uint32_t rgb) override;
  void drawCircle(int16_t x, int16_t y, int16_t radius,
                  uint32_t rgb) override;
  void setOutputScale(float scale) override;
  void show() override;

 private:
  uint16_t to565(uint32_t rgb) const;

  Adafruit_NeoMatrix& matrix_;
  float outputScale_ = 1.0f;
};

#endif
