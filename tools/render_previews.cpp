// Renders one gallery entry with the firmware's own code: native engine nENGINE
// or collection variant pVARIANT, at its compiled defaults and palette, for eight
// seconds at 60 FPS, writing every second frame as RGB888 rows.
#include "../src/patterns/ArtDirection.h"
#include "../src/patterns/NativeEngines.h"
#include "../src/patterns/Palettes.h"
#include "../src/patterns/PatternflowPack.h"
#include "../src/patterns/Rgb565.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

struct Frame { int size; uint16_t pixels[64 * 64]; };
void put(void* frame, int16_t x, int16_t y, uint16_t color) {
  auto& f = *static_cast<Frame*>(frame);
  if (x >= 0 && y >= 0 && x < f.size && y < f.size) f.pixels[y * f.size + x] = color;
}

int main(int argc, char** argv) {
  const int size = argc == 4 ? atoi(argv[2]) : 0, id = argc == 4 ? atoi(argv[1] + 1) : -1;
  const bool collection = argc == 4 && argv[1][0] == 'p', native = argc == 4 && argv[1][0] == 'n';
  if ((!collection && !native) || size < 1 || size > 64 || id < 0 ||
      (collection ? id >= PATTERNFLOW_PATTERN_COUNT || !PATTERNFLOW_METADATA[id].name : id >= 16)) {
    fprintf(stderr, "usage: render_previews nENGINE|pVARIANT SIZE OUTPUT.rgb\n");
    return 1;
  }
  FILE* out = fopen(argv[3], "wb");
  if (!out) return 2;
  const int engine = collection ? 16 : id, variant = collection ? id : 0;
  const int palette = Palettes::forPattern(engine, variant);
  const uint32_t* stops = palette != Palettes::Original ? Palettes::Stops[palette]
                        : collection ? PATTERNFLOW_METADATA[id].originalColors : ArtDirection::Native[id];
  uint16_t table[256];
  for (int i = 0; i < 256; i++) {
    const float position = i / 64.f, mix = position - int(position);
    const uint32_t a = stops[int(position) & 3], b = stops[(int(position) + 1) & 3];
    float c[3];
    for (int k = 0; k < 3; k++) c[k] = (((a >> (16 - 8 * k)) & 255) * (1 - mix) + ((b >> (16 - 8 * k)) & 255) * mix) / 255.f;
    table[i] = rgb565(c[0], c[1], c[2]);
  }
  const SourceColors source{table, 1.f, 1.f, palette == Palettes::Original};
  PatternflowPack pack;
  NativeEngines natives;
  uint64_t controls[4] = {};
  if (native) nativeDefaults(engine, controls);
  pack.reset(variant);
  natives.reset();
  static Frame frame;
  frame.size = size;
  for (int f = 0; f < 480; f++) {
    memset(frame.pixels, 0, sizeof(frame.pixels));
    if (collection) pack.render(variant, size, size, 1.f / 60, PATTERNFLOW_METADATA[id].defaults, table, source, put, &frame);
    else natives.render(engine, size, size, 1.f / 60, controls, table, 1.f, put, &frame);
    if (f % 2) continue;
    for (int i = 0; i < size * size; i++) {
      const uint16_t p = frame.pixels[i];
      const unsigned char rgb[3] = {(unsigned char)(((p >> 11) & 31) * 255 / 31), (unsigned char)(((p >> 5) & 63) * 255 / 63),
                                    (unsigned char)((p & 31) * 255 / 31)};
      fwrite(rgb, 1, 3, out);
    }
  }
  pack.reset(0);
  fclose(out);
  return 0;
}
