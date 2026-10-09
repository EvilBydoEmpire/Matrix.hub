#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#define PROGMEM
#define PI 3.14159265358979323846
using std::min;
using std::max;
template<class T, class L, class H> inline T constrain(T v, L low, H high) {
  return v < low ? T(low) : (v > high ? T(high) : v);
}
