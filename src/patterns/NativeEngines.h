#pragma once
#include <stdint.h>
#include "PatternflowPack.h"

// Stable engine IDs; retired engines keep their numbers.
enum Animation {
  STARFIELD,
  WARPED_CHECKERBOARD,
  MOIRE_RINGS,          // Retired
  MOIRE_LINES,
  WARPING_CHECKERS,     // Retired
  SHOCKWAVE,
  WARP_CORRIDORS,
  PIXEL_AVALANCHE,
  KALEIDOSCOPE,         // Retired
  BAYER_FLOW,
  FLOYD_FLOW,           // Retired
  SNAKE,
  SQUARES,
  PERSPECTIVE_FLOW,     // Retired
  FLOW_SPHERE,
  FLOW_FIELD_2D,        // Retired
  PATTERNFLOW_COLLECTION
};

// Each engine's controls; a pattern's stored record copies them, so their
// layouts stay fixed.
struct StarfieldParams          { int n; float s, w; };
struct WarpedCheckerboardParams { float as, ws, d; };
struct MoireLinesParams         { float as, rs1, rs2, lf; };
struct ShockwaveParams          { float as, wt; };
struct WarpCorridorsParams      { float as, rs, ws, bf; };
struct PixelAvalancheParams     { int sr; float as; };
struct BayerFlowParams          { int scale, contrast; float as; };
struct SnakeParams              { int length; float speed, steering; };
struct SquaresParams            { int count, thickness; float as, spacing, variance; };
struct FlowSphereParams         { float speed, scale, density, radius; };
constexpr int MAX_STARS = 300;

// Copies an engine's compiled defaults into its controls.
void nativeDefaults(int engine, void* controls);

// The ten native engines, drawn at any panel size up to 64×64 from an engine's
// controls and the playing palette's table, for the firmware and the host
// thumbnail renderer alike.
class NativeEngines {
 public:
  // Starts the motion over.
  void reset();
  // Draws one frame; controls points to the engine's parameter struct above,
  // and colorScale is the share of full scale the palette table is drawn at,
  // which Snake's fixed colours follow.
  void render(int engine, uint16_t width, uint16_t height, float deltaTime,
              const void* controls, const uint16_t palette[256], float colorScale,
              PatternflowPixelWriter writer, void* context);
};
