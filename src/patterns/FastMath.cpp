#include "FastMath.h"

int16_t fastSinTable[FAST_SIN_SIZE];

void ensureFastMath() {
  static bool ready = false;
  if (ready) return;
  for (uint16_t i = 0; i < FAST_SIN_SIZE; i++)
    fastSinTable[i] = (int16_t)lroundf(sinf(i * (FAST_TAU / FAST_SIN_SIZE)) * 32767.f);
  ready = true;
}
