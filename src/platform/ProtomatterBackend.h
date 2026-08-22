#pragma once

#include <Adafruit_Protomatter.h>

#include "DisplayBackend.h"

class ProtomatterBackend final : public DisplayBackend {
 public:
  explicit ProtomatterBackend(Adafruit_Protomatter& matrix);

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
  void show() override;

 private:
  uint16_t to565(uint32_t rgb) const;

  Adafruit_Protomatter& matrix_;
};

