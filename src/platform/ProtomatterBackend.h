#pragma once

#if !defined(MATRIX_PROFILE_WS2812)
#include "PanelOrientation.h"

#if defined(MATRIX_HUB75_DMA)
#include "DmaMatrix.h"
using Hub75Canvas = DmaMatrix;
#else
#include <Adafruit_Protomatter.h>
using Hub75Canvas = Adafruit_Protomatter;
#endif

#include "DisplayBackend.h"

class ProtomatterBackend final : public DisplayBackend {
 public:
  explicit ProtomatterBackend(Hub75Canvas& matrix);

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
  bool setChannelBalance(uint8_t red, uint8_t green, uint8_t blue);

 private:
  uint16_t to565(uint32_t rgb) const;

  Hub75Canvas& matrix_;
  float outputScale_ = 1.0f;
  uint16_t* uncorrected_ = nullptr;
  uint8_t balance_[3] = {100, 100, 100};
};

#endif
