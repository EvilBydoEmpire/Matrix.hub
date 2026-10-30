#include "Hub75Backend.h"

#if !defined(MATRIX_PROFILE_WS2812)
#include "ColorCalibration.h"

Hub75Backend::Hub75Backend(DmaMatrix& matrix) : matrix_(matrix) {}

bool Hub75Backend::setChannelBalance(uint8_t red, uint8_t green, uint8_t blue) {
  if (red > 100 || green > 100 || blue > 100) return false;
  if ((red != 100 || green != 100 || blue != 100) && !uncorrected_) {
    uncorrected_ = (uint16_t*)malloc(matrix_.width() * matrix_.height() * sizeof(uint16_t));
    if (!uncorrected_) return false;
  }
  balance_[0] = red;
  balance_[1] = green;
  balance_[2] = blue;
  return true;
}

// Balances the frame, then restores the source so repeated show() calls never
// compound the gains.
void Hub75Backend::show() {
  if (balance_[0] == 100 && balance_[1] == 100 && balance_[2] == 100) {
    matrix_.show();
    return;
  }
  if (!uncorrected_) return;  // Keep the last valid frame.
  const size_t count = matrix_.width() * matrix_.height();
  uint16_t* pixels = matrix_.getBuffer();
  memcpy(uncorrected_, pixels, count * sizeof(uint16_t));
  for (size_t i = 0; i < count; i++)
    pixels[i] = channelBalanced565(uncorrected_[i], balance_[0], balance_[1], balance_[2]);
  matrix_.show();
  memcpy(pixels, uncorrected_, count * sizeof(uint16_t));
}

#endif
