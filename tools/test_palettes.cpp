#include <cassert>
#include <cmath>
#include <cstdint>
#include "../src/patterns/PatternflowPack.h"
#include "../src/patterns/Palettes.h"
#include "../src/patterns/PatternCatalog.h"

static double luminance(uint32_t c) {
  auto linear = [](double v) { v /= 255; return v <= .04045 ? v / 12.92 : std::pow((v + .055) / 1.055, 2.4); };
  return .2126 * linear((c >> 16) & 255) + .7152 * linear((c >> 8) & 255) + .0722 * linear(c & 255);
}

int main() {
  static_assert(sizeof(Palettes::Variants) == PATTERNFLOW_PATTERN_COUNT, "one default per stable variant");
  static_assert(Palettes::Count <= 255, "Pattern records keep the palette ID in one byte");

  // Named palettes run shadow < body < accent < highlight, so cyclic sampling
  // wraps from accent back to shadow and two-role patterns contrast stops 1/3.
  int named = 0;
  for (int p = 0; p < Palettes::Count; p++) {
    const uint32_t* s = Palettes::Stops[p];
    if (!Palettes::offered(p) || p == Palettes::Original) {
      for (int c = 0; c < 4; c++) assert(s[c] == 0);
      continue;
    }
    named++;
    for (int c = 0; c < 4; c++) assert(s[c] != 0);
    assert(luminance(s[0]) < luminance(s[1]));
    assert(luminance(s[1]) < luminance(s[3]));
    assert(luminance(s[3]) < luminance(s[2]));
  }
  assert(named == 29);
  for (int p = 11; p <= 20; p++) assert(!Palettes::offered(p));  // Retired IDs stay reserved.

  // The app's order lists every offered palette once, Original last.
  bool listed[Palettes::Count] = {};
  for (uint8_t id : Palettes::Order) { assert(Palettes::offered(id) && !listed[id]); listed[id] = true; }
  for (int p = 0; p < Palettes::Count; p++) assert(listed[p] == Palettes::offered(p));
  assert(Palettes::Order[sizeof(Palettes::Order) - 1] == Palettes::Original);

  // Every pattern, active or retired, defaults to an offered palette.
  for (int e = 0; e < 16; e++) assert(Palettes::offered(Palettes::forPattern(e, 0)));
  for (int v = 0; v < PATTERNFLOW_PATTERN_COUNT; v++) assert(Palettes::offered(Palettes::forPattern(16, v)));
  // The active gallery uses named palettes by default; Original stays a choice.
  for (int i = 0; i < CORE_PATTERN_COUNT; i++) assert(Palettes::forPattern(CORE_PATTERN_IDS[i], 0) != Palettes::Original);
  for (int i = 0; i < COLLECTION_PATTERN_COUNT; i++) assert(Palettes::forPattern(16, COLLECTION_PATTERN_IDS[i]) != Palettes::Original);
  assert(Palettes::forPattern(16, -1) == 0 && Palettes::forPattern(16, 999) == 0 && Palettes::forPattern(99, 0) == 0);
  return 0;
}
