#include "ProtomatterBackend.h"

ProtomatterBackend::ProtomatterBackend(Adafruit_Protomatter& matrix)
    : matrix_(matrix) {}

uint16_t ProtomatterBackend::width() const { return matrix_.width(); }

uint16_t ProtomatterBackend::height() const { return matrix_.height(); }

void ProtomatterBackend::clear(uint32_t rgb) {
  matrix_.fillScreen(to565(rgb));
}

void ProtomatterBackend::setPixel(int16_t x, int16_t y, uint32_t rgb) {
  matrix_.drawPixel(x, y, to565(rgb));
}

void ProtomatterBackend::drawLine(int16_t x0, int16_t y0, int16_t x1,
                                  int16_t y1, uint32_t rgb) {
  matrix_.drawLine(x0, y0, x1, y1, to565(rgb));
}

void ProtomatterBackend::drawRect(int16_t x, int16_t y, int16_t w, int16_t h,
                                  uint32_t rgb) {
  matrix_.drawRect(x, y, w, h, to565(rgb));
}

void ProtomatterBackend::fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                                  uint32_t rgb) {
  matrix_.fillRect(x, y, w, h, to565(rgb));
}

void ProtomatterBackend::drawCircle(int16_t x, int16_t y, int16_t radius,
                                    uint32_t rgb) {
  matrix_.drawCircle(x, y, radius, to565(rgb));
}

void ProtomatterBackend::show() { matrix_.show(); }

uint16_t ProtomatterBackend::to565(uint32_t rgb) const {
  const uint8_t red = (rgb >> 16) & 0xFF;
  const uint8_t green = (rgb >> 8) & 0xFF;
  const uint8_t blue = rgb & 0xFF;
  return matrix_.color565(red, green, blue);
}

