#pragma once

// Physical mounting orientation of the HUB75 panel, applied as each frame
// leaves for the panel.
#ifndef MATRIX_OUTPUT_ROTATION
#define MATRIX_OUTPUT_ROTATION 1
#endif
static_assert(MATRIX_OUTPUT_ROTATION>=0 && MATRIX_OUTPUT_ROTATION<=3,
              "Panel orientation must be 0..3 clockwise quarter turns");
constexpr int panelOutputIndex(int x,int y,int size,int quarterTurns) {
  return quarterTurns==1 ? x*size+(size-1-y) :
         quarterTurns==2 ? (size-1-y)*size+(size-1-x) :
         quarterTurns==3 ? (size-1-x)*size+y : y*size+x;
}
