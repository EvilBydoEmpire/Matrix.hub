#include "Ws2812Backend.h"

#if defined(MATRIX_PROFILE_WS2812)

Ws2812Backend::Ws2812Backend(Adafruit_NeoMatrix& matrix) : matrix_(matrix) {}

uint16_t Ws2812Backend::width() const { return matrix_.width(); }

uint16_t Ws2812Backend::height() const { return matrix_.height(); }

void Ws2812Backend::clear(uint32_t rgb) { matrix_.fillScreen(to565(rgb)); }

void Ws2812Backend::setPixel(int16_t x, int16_t y, uint32_t rgb) {
  matrix_.drawPixel(x, y, to565(rgb));
}

void Ws2812Backend::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                             uint32_t rgb) {
  matrix_.drawLine(x0, y0, x1, y1, to565(rgb));
}

void Ws2812Backend::drawRect(int16_t x, int16_t y, int16_t w, int16_t h,
                             uint32_t rgb) {
  matrix_.drawRect(x, y, w, h, to565(rgb));
}

void Ws2812Backend::fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                             uint32_t rgb) {
  matrix_.fillRect(x, y, w, h, to565(rgb));
}

void Ws2812Backend::drawCircle(int16_t x, int16_t y, int16_t radius,
                               uint32_t rgb) {
  matrix_.drawCircle(x, y, radius, to565(rgb));
}

void Ws2812Backend::setOutputScale(float scale) {
  outputScale_ = constrain(scale, 0.0f, 1.0f);
}

void Ws2812Backend::show() {
  matrix_.show();
}

uint16_t Ws2812Backend::to565(uint32_t rgb) const {
  return matrix_.Color(((rgb >> 16) & 0xFF) * outputScale_,
                       ((rgb >> 8) & 0xFF) * outputScale_,
                       (rgb & 0xFF) * outputScale_);
}

#endif
