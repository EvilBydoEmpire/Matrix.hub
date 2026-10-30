#pragma once

#if !defined(MATRIX_PROFILE_WS2812)
#include "DmaMatrix.h"

// Shows HUB75 frames through the panel's white balance.
class Hub75Backend {
 public:
  explicit Hub75Backend(DmaMatrix& matrix);

  void show();
  bool setChannelBalance(uint8_t red, uint8_t green, uint8_t blue);

 private:
  DmaMatrix& matrix_;
  uint16_t* uncorrected_ = nullptr;
  uint8_t balance_[3] = {100, 100, 100};
};

#endif
