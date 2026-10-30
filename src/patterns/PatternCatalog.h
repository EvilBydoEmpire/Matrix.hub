#pragma once
#include <stdint.h>
#include "PatternflowPack.h"

// Stable pattern IDs: a native engine ID, or 16 + a collection variant. Settings
// key on them, so retired IDs stay reserved; thumbnails follow gallery order.
constexpr int PATTERN_ID_COUNT=16+PATTERNFLOW_PATTERN_COUNT;
constexpr int patternId(int engine,int variant){return engine==16?16+variant:engine;}

// Gallery order.
constexpr uint8_t CORE_PATTERN_COUNT=10;
constexpr uint8_t COLLECTION_PATTERN_COUNT=52;
constexpr uint8_t GALLERY_PATTERN_COUNT=CORE_PATTERN_COUNT+COLLECTION_PATTERN_COUNT;
const uint8_t CORE_PATTERN_IDS[CORE_PATTERN_COUNT]={0,1,3,5,6,7,9,11,12,14};
const uint8_t COLLECTION_PATTERN_IDS[COLLECTION_PATTERN_COUNT]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,18,19,21,22,23,25,29,31,33,35,36,38,42,43,45,46,47,48,49,50,51,52,53,54,55,58,59,60,61,62,63,64,65,66,67};

// Flashing patterns play only when picked: cycling and boot skip them.
constexpr bool isFlashingVariant(int variant){return variant>=65&&variant<=67;}
// The gallery index cycling plays after index.
inline int nextCycledIndex(int index){
 int next=index;
 do next=(next+1)%GALLERY_PATTERN_COUNT;
 while(next>=CORE_PATTERN_COUNT&&isFlashingVariant(COLLECTION_PATTERN_IDS[next-CORE_PATTERN_COUNT]));
 return next;
}
