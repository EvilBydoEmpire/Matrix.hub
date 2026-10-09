#pragma once
// Stable engine/variant identities; active gallery order is intentionally dense.
constexpr uint8_t CORE_PATTERN_COUNT=10;
constexpr uint8_t COLLECTION_PATTERN_COUNT=49;
constexpr uint8_t GALLERY_PATTERN_COUNT=CORE_PATTERN_COUNT+COLLECTION_PATTERN_COUNT;
const uint8_t CORE_PATTERN_IDS[CORE_PATTERN_COUNT]={0,1,3,5,6,7,9,11,12,14};
const uint8_t COLLECTION_PATTERN_IDS[COLLECTION_PATTERN_COUNT]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,18,19,21,22,23,25,29,31,33,35,36,38,42,43,45,46,47,48,49,50,51,52,53,54,55,58,59,60,61,62,63,64};
const uint8_t LEGACY_VARIANT_IDS[17]={0,1,8,9,13,14,17,18,20,21,23,27,31,32,35,36,37};
// Keep persisted bookmark slots 52/53 reserved after removing those entries.
constexpr uint8_t bookmarkStorageIndex(uint8_t galleryIndex){return galleryIndex<52?galleryIndex:galleryIndex+2;}
static_assert(GALLERY_PATTERN_COUNT+2<=64,"Expand bookmark storage before adding more entries");
static_assert(PATTERNFLOW_PATTERN_COUNT<=80,"Expand quality and metadata storage before adding more variants");
