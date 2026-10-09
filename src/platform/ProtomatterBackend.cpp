#include "ProtomatterBackend.h"
#include "ColorCalibration.h"

#if !defined(MATRIX_PROFILE_WS2812)
#include "PanelOrientation.h"

ProtomatterBackend::ProtomatterBackend(Hub75Canvas& matrix)
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

void ProtomatterBackend::setOutputScale(float scale) {
  outputScale_ = constrain(scale, 0.0f, 1.0f);
}

bool ProtomatterBackend::setChannelBalance(uint8_t red,uint8_t green,uint8_t blue) {
  if(red>100 || green>100 || blue>100)return false;
  if((red!=100 || green!=100 || blue!=100) && !uncorrected_) {
    uncorrected_=(uint16_t*)malloc(width()*height()*sizeof(uint16_t));
    if(!uncorrected_)return false;
  }
  balance_[0]=red;balance_[1]=green;balance_[2]=blue;
  return true;
}
void ProtomatterBackend::show() {
  const bool neutral=balance_[0]==100 && balance_[1]==100 && balance_[2]==100;
#if defined(MATRIX_HUB75_DMA)
  constexpr int rotation=0; // DMA converter applies orientation once.
#else
  constexpr int rotation=MATRIX_OUTPUT_ROTATION;
#endif
  if(neutral && rotation==0) {
    matrix_.show();return;
  }
  if(!uncorrected_)uncorrected_=(uint16_t*)malloc(width()*height()*sizeof(uint16_t));
  if(!uncorrected_)return; // Keep the last valid frame if allocation fails.
  // Correct every renderer (including Lua/direct framebuffer paths). Restore
  // the source afterwards so repeated show() calls never compound the gains.
  const size_t count=width()*height();uint16_t* pixels=matrix_.getBuffer();
  memcpy(uncorrected_,pixels,count*sizeof(uint16_t));
  for(int y=0;y<height();y++)for(int x=0;x<width();x++){
    const uint16_t source=uncorrected_[y*width()+x];
    pixels[panelOutputIndex(x,y,width(),rotation)]=neutral?source:
      channelBalanced565(source,balance_[0],balance_[1],balance_[2]);
  }
  matrix_.show();
  memcpy(pixels,uncorrected_,count*sizeof(uint16_t));
}

uint16_t ProtomatterBackend::to565(uint32_t rgb) const {
  const uint8_t red = ((rgb >> 16) & 0xFF) * outputScale_;
  const uint8_t green = ((rgb >> 8) & 0xFF) * outputScale_;
  const uint8_t blue = (rgb & 0xFF) * outputScale_;
  return matrix_.color565(red, green, blue);
}

#endif
