#pragma once
#include <stdint.h>
// Original colours of the native engines, by engine ID.
// Duplicate anchors intentionally produce one/two-colour treatments.
namespace ArtDirection {
constexpr uint32_t Native[16][4]={
 {0xB8D7EB,0xE9E5D8,0xB8D7EB,0xE9E5D8}, // Starfield: cool stars / ivory
 {0x131326,0x7376C8,0xE2DECA,0x7376C8}, // Checkerboard: ink / periwinkle / ivory
 {}, // Retired 2
 {0x214878,0x69C9D2,0xDBE5D6,0x69C9D2}, // Moire: blue / silver
 {}, // Retired 4
 {0xB46B39,0xF2DCA6,0xB46B39,0xF2DCA6}, // Shockwave: amber / parchment
 {0x154352,0x68CCC2,0xDFE7D3,0x68CCC2}, // Corridors: petrol / mint
 {0xE9E5D8,0xE9E5D8,0xE9E5D8,0xE9E5D8}, // Avalanche: ivory, black from geometry
 {}, // Retired 8
 {0x1D5260,0x73C8B6,0xECD9AF,0x73C8B6}, // Bayer Flow: jade / cream
 {}, // Retired 10
 {0x32E34E,0x32E34E,0x32E34E,0x32E34E}, // Snake: green, apple remains red
 {0x588AB5,0xBCCDCF,0xDB9D72,0xBCCDCF}, // Squares: steel / copper
 {}, // Retired 13
 {0x123C54,0x36ACAE,0xC9E7D1,0x36ACAE}, // Flow: deep water / foam
 {} // Retired 15
};
}
