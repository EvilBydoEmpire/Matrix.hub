#include "NativeEngines.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "FastMath.h"
#include "Rgb565.h"

namespace {
#if defined(MATRIX_PROFILE_WS2812) && defined(WS2812_MATRIX_WIDTH) && defined(WS2812_MATRIX_HEIGHT)
constexpr int MAX_PIXELS = WS2812_MATRIX_WIDTH * WS2812_MATRIX_HEIGHT;
#else
constexpr int MAX_PIXELS = 64 * 64;
#endif
constexpr int MAX_SNAKE_SEGMENTS = 512;
const uint8_t BAYER_8X8[64] = {
   0,48,12,60, 3,51,15,63,
  32,16,44,28,35,19,47,31,
   8,56, 4,52,11,59, 7,55,
  40,24,36,20,43,27,39,23,
   2,50,14,62, 1,49,13,61,
  34,18,46,30,33,17,45,29,
  10,58, 6,54, 9,57, 5,53,
  42,26,38,22,41,25,37,21
};

// The panel being drawn, its distances from the centre and where the writer goes.
int W = 0, H = 0;
float distanceMap[MAX_PIXELS];
PatternflowPixelWriter output = nullptr;
void* outputContext = nullptr;
PIXEL_INLINE inline void put(int x, int y, uint16_t color) {
  if (x >= 0 && y >= 0 && x < W && y < H) output(outputContext, x, y, color);
}

// Phases the engines integrate from their rate controls, so moving a control
// changes the speed without jumping the motion.
float enginePhase[3] = {};
float squarePhase[20] = {};

uint32_t seed = 1;
inline uint32_t nextRandom() { return seed = seed * 1664525UL + 1013904223UL; }
inline int randomBelow(int n) { return n > 0 ? int((nextRandom() >> 8) % uint32_t(n)) : 0; }
inline int randomBetween(int low, int high) { return low + randomBelow(high - low); }

struct Star { float x, y, z; };
Star stars[MAX_STARS];
bool starsPlaced = false;
void placeStar(Star& star) {
  star = {randomBetween(-W, W) * 1.0f, randomBetween(-H, H) * 1.0f, randomBetween(1, W) * 1.0f};
}

uint8_t avalancheGrid[MAX_PIXELS] = {};
float avalancheTimeSinceUpdate = 0;
bool avalancheWhitePhase = true;

float flowSphereTime = 0.0f;

int8_t snakeX[MAX_SNAKE_SEGMENTS] = {};
int8_t snakeY[MAX_SNAKE_SEGMENTS] = {};
int snakeLength = 0;
int snakeDirection = 0;
int snakeAppleX = 0;
int snakeAppleY = 0;
float snakeMoveAccumulator = 0.0f;

bool snakeContains(int x, int y, int count = -1) {
  const int limit = count < 0 ? snakeLength : min(count, snakeLength);
  for (int i = 0; i < limit; i++) {
    if (snakeX[i] == x && snakeY[i] == y) return true;
  }
  return false;
}

void placeSnakeApple() {
  for (int attempt = 0; attempt < 256; attempt++) {
    const int x = randomBelow(W);
    const int y = randomBelow(H);
    if (!snakeContains(x, y)) {
      snakeAppleX = x;
      snakeAppleY = y;
      return;
    }
  }
  snakeAppleX = 0;
  snakeAppleY = 0;
}

void resetSnake(const SnakeParams& params) {
  snakeLength = min(constrain(params.length, 4, MAX_SNAKE_SEGMENTS), min(12, W - 2));
  snakeLength = max(4, snakeLength);
  snakeDirection = 0;
  snakeMoveAccumulator = 0.0f;
  const int headX = max(snakeLength + 1, W / 2);
  const int headY = H / 2;
  for (int i = 0; i < snakeLength; i++) {
    snakeX[i] = headX - i;
    snakeY[i] = headY;
  }
  placeSnakeApple();
}

bool advanceSnake(const SnakeParams& params) {
  static const int8_t dx[4] = {1, 0, -1, 0};
  static const int8_t dy[4] = {0, 1, 0, -1};
  int safeDirections[3];
  int safeCount = 0;
  int bestDirection = -1;
  int bestDistance = 10000;

  for (int direction = 0; direction < 4; direction++) {
    if (direction == ((snakeDirection + 2) & 3)) continue;
    const int nx = snakeX[0] + dx[direction];
    const int ny = snakeY[0] + dy[direction];
    if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
    if (snakeContains(nx, ny, max(0, snakeLength - 1))) continue;
    safeDirections[safeCount++] = direction;
    const int distance = abs(nx - snakeAppleX) + abs(ny - snakeAppleY);
    if (distance < bestDistance) {
      bestDistance = distance;
      bestDirection = direction;
    }
  }

  if (safeCount == 0) return false;
  const bool steerTowardApple = randomBelow(1000) < (int)(params.steering * 1000.0f);
  snakeDirection = steerTowardApple && bestDirection >= 0
    ? bestDirection
    : safeDirections[randomBelow(safeCount)];

  const int newX = snakeX[0] + dx[snakeDirection];
  const int newY = snakeY[0] + dy[snakeDirection];
  const bool ateApple = newX == snakeAppleX && newY == snakeAppleY;
  const int maximumLength = min(min(params.length, MAX_SNAKE_SEGMENTS), W * H - 1);
  int newLength = snakeLength;
  if (ateApple && newLength < maximumLength) newLength++;
  if (newLength > maximumLength) newLength--;
  for (int i = min(newLength - 1, MAX_SNAKE_SEGMENTS - 1); i > 0; i--) {
    snakeX[i] = snakeX[i - 1];
    snakeY[i] = snakeY[i - 1];
  }
  snakeX[0] = newX;
  snakeY[0] = newY;
  snakeLength = newLength;
  if (ateApple) placeSnakeApple();
  return true;
}
}  // namespace

void nativeDefaults(int engine, void* controls) {
  switch (engine) {
    case STARFIELD:           *static_cast<StarfieldParams*>(controls)          = {120, 4.0f, 0.2f}; break;
    case WARPED_CHECKERBOARD: *static_cast<WarpedCheckerboardParams*>(controls) = {1.0f, 5.0f, 4.0f}; break;
    case MOIRE_LINES:         *static_cast<MoireLinesParams*>(controls)         = {1.0f, 1.5f, 1.2f, 0.6f}; break;
    case SHOCKWAVE:           *static_cast<ShockwaveParams*>(controls)          = {1.0f, 2.0f}; break;
    case WARP_CORRIDORS:      *static_cast<WarpCorridorsParams*>(controls)      = {1.0f, 0.25f, 0.6f, 1.0f}; break;
    case PIXEL_AVALANCHE:     *static_cast<PixelAvalancheParams*>(controls)     = {4, 1.0f}; break;
    case BAYER_FLOW:          *static_cast<BayerFlowParams*>(controls)          = {7, 4, 1.0f}; break;
    case SNAKE:               *static_cast<SnakeParams*>(controls)              = {96, 8.0f, 0.82f}; break;
    case SQUARES:             *static_cast<SquaresParams*>(controls)            = {10, 1, 1.0f, 1.0f, 0.16f}; break;
    case FLOW_SPHERE:         *static_cast<FlowSphereParams*>(controls)         = {0.12f, 1.45f, 0.8f, 28.0f}; break;
  }
}

void NativeEngines::reset() {
  memset(enginePhase, 0, sizeof(enginePhase));
  memset(squarePhase, 0, sizeof(squarePhase));
  memset(avalancheGrid, 0, sizeof(avalancheGrid));
  avalancheTimeSinceUpdate = 0;
  avalancheWhitePhase = true;
  flowSphereTime = 0.0f;
  snakeLength = 0;
  seed = 1;
  starsPlaced = false;
}

void NativeEngines::render(int engine, uint16_t width, uint16_t height, float deltaTime,
                           const void* controls, const uint16_t palette[256], float colorScale,
                           PatternflowPixelWriter writer, void* context) {
  if (!controls || !writer || width == 0 || height == 0 || width > 64 || height > 64 ||
      width * height > MAX_PIXELS) return;
  ensureFastMath();
  output = writer;
  outputContext = context;
  if (W != width || H != height) {
    W = width;
    H = height;
    const float centerX = W / 2.0f - 0.5f;
    const float centerY = H / 2.0f - 0.5f;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++)
        distanceMap[y * W + x] = sqrtf((x - centerX) * (x - centerX) + (y - centerY) * (y - centerY));
    memset(avalancheGrid, 0, sizeof(avalancheGrid));
    snakeLength = 0;
    starsPlaced = false;
  }
  // Native engines size their features to the panel, relative to 32 pixels.
  const float displayScale = W / 32.0f;
  const float cx = (W - 1) * 0.5f;
  const float cy = (H - 1) * 0.5f;

  switch (engine) {
    case STARFIELD: {
      const StarfieldParams& sfParams = *static_cast<const StarfieldParams*>(controls);
      if (!starsPlaced) {
        for (Star& star : stars) placeStar(star);
        starsPlaced = true;
      }
      // Flight speed and the number of stars follow the panel's width: a smaller
      // panel shows the same flight with fewer stars.
      const float panelShare = W / 64.0f;
      const int starCount = constrain((int)lroundf(sfParams.n * panelShare), 1, MAX_STARS);
      const float hueDrift = (enginePhase[0] += deltaTime * 0.015f);
      for (int i = 0; i < starCount; i++) {
        stars[i].z -= sfParams.w * 50.0f * panelShare * deltaTime;
        if (stars[i].z < 1.0f) placeStar(stars[i]);

        const int sx = (int)lroundf((stars[i].x / stars[i].z) * (W / sfParams.s) + cx);
        const int sy = (int)lroundf((stars[i].y / stars[i].z) * (H / sfParams.s) + cy);

        float nearFactor = 1.0f - constrain(stars[i].z / W, 0.0f, 1.0f);
        float hue = 0.56f + i * 0.007f + hueDrift;
        put(sx, sy, paletteColorAt(palette, hue, 0.35f + 0.65f * nearFactor));
      }
      break;
    }

    case WARPED_CHECKERBOARD: {
      const WarpedCheckerboardParams& wcParams = *static_cast<const WarpedCheckerboardParams*>(controls);
      const float time = (enginePhase[0] += deltaTime * wcParams.as);
      const float inverseWaveScale = 1.0f / max(1.0f, wcParams.ws * displayScale);
      const float distortion = wcParams.d * displayScale;

      for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
          const float warp = fastSin(distanceMap[y * W + x] * inverseWaveScale - time * 3.0f) * distortion;
          const bool on = (((x + y + floorInt(warp)) & 1) == 0);
          if (on) {
            const float hue = 0.69f + 0.055f * fastSin(time * 0.22f + distanceMap[y * W + x] * 0.075f);
            put(x, y, paletteColorAt(palette, hue, 199 / 255.0f));
          }
        }
      }
      break;
    }

    case MOIRE_LINES: {
      const MoireLinesParams& mlParams = *static_cast<const MoireLinesParams*>(controls);
      const float time = (enginePhase[0] += deltaTime * mlParams.as);
      const float angle1 = (enginePhase[1] += deltaTime * mlParams.as * mlParams.rs1);
      const float angle2 = -(enginePhase[2] += deltaTime * mlParams.as * mlParams.rs2);
      const float c1 = cosf(angle1), s1 = sinf(angle1);
      const float c2 = cosf(angle2), s2 = sinf(angle2);
      // Tuned at 64 pixels. Hard-edged lines alias at full scale on a smaller
      // panel, so their frequency rises only by the square root.
      const float spatial = max(0.04f, mlParams.lf * 0.42f) * sqrtf(64.0f / W);

      for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
          const float px = x - cx;
          const float py = y - cy;
          const float v1 = px * c1 + py * s1;
          const float v2 = px * c2 + py * s2;
          const bool wave1 = fastSin(v1 * spatial + time * 0.18f) > 0.0f;
          const bool wave2 = fastSin(v2 * spatial - time * 0.15f) > 0.0f;

          if (wave1 != wave2) {
            const float hue = 0.57f + (v1 - v2) * 0.0035f + time * 0.010f;
            put(x, y, paletteColorAt(palette, hue, 194 / 255.0f));
          }
        }
      }
      break;
    }

    case SHOCKWAVE: {
      const ShockwaveParams& swParams = *static_cast<const ShockwaveParams*>(controls);
      const float time = (enginePhase[0] += deltaTime * swParams.as);
      const float inverseWavelength = 1.0f / max(0.5f, swParams.wt * displayScale);
      for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
          const float wave = fastSin(distanceMap[y * W + x] * inverseWavelength - time * 2.5f);
          const int phase = (wave > 0.0f) ? 1 : 0;
          if (((x + y) & 1) == phase) {
            const float hue = 0.02f + 0.08f * (wave + 1.0f) + time * 0.006f;
            put(x, y, paletteColorAt(palette, hue, 0.58f + fabsf(wave) * 0.24f));
          }
        }
      }
      break;
    }

    case WARP_CORRIDORS: {
      const WarpCorridorsParams& wcoParams = *static_cast<const WarpCorridorsParams*>(controls);
      const float time = (enginePhase[0] += deltaTime * wcoParams.as);
      const float angle = (enginePhase[1] += deltaTime * wcoParams.as * wcoParams.rs);
      const float sinA = sinf(angle), cosA = cosf(angle);
      const float breathing = 1.3f + 0.7f * sinf(enginePhase[2] += deltaTime * wcoParams.as * wcoParams.ws);
      // Tuned at 64 pixels; smaller panels show the same composition.
      const float spatial = max(0.015f, 0.20f * wcoParams.bf * breathing) * 64.0f / W;
      const float phase = time * 2.4f;

      for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
          const float px = x - cx;
          const float py = y - cy;
          const float rotX = px * cosA - py * sinA;
          const float rotY = px * sinA + py * cosA;
          const float perspective = 1.0f + fabsf(rotY) * 0.012f * 64.0f / W;
          const float wave = fastSin(rotX * spatial * perspective + phase);

          if (wave > 0.0f) {
            const float hue = 0.51f + rotY * 0.0025f + time * 0.010f;
            put(x, y, paletteColorAt(palette, hue, 0.62f + 0.22f * wave));
          }
        }
      }
      break;
    }

    case PIXEL_AVALANCHE: {
      const PixelAvalancheParams& paParams = *static_cast<const PixelAvalancheParams*>(controls);
      avalancheTimeSinceUpdate += deltaTime;
      const float updateInterval = 1.0f / max(1.0f, 18.0f * paParams.as);

      while (avalancheTimeSinceUpdate >= updateInterval) {
        avalancheTimeSinceUpdate -= updateInterval;

        for (int i = 0; i < paParams.sr; i++) {
          const int spawnX = randomBelow(W);
          if (avalancheGrid[spawnX] == 0) avalancheGrid[spawnX] = 1;
        }

        for (int y = H - 2; y >= 0; y--) {
          for (int x = 0; x < W; x++) {
            if (avalancheGrid[y * W + x] == 0) continue;

            if (avalancheGrid[(y + 1) * W + x] == 0) {
              avalancheGrid[y * W + x] = 0;
              avalancheGrid[(y + 1) * W + x] = 1;
              continue;
            }

            const int dir = randomBelow(2) ? 1 : -1;
            const int nx1 = x + dir;
            const int nx2 = x - dir;
            if (nx1 >= 0 && nx1 < W && avalancheGrid[(y + 1) * W + nx1] == 0) {
              avalancheGrid[y * W + x] = 0;
              avalancheGrid[(y + 1) * W + nx1] = 1;
            } else if (nx2 >= 0 && nx2 < W && avalancheGrid[(y + 1) * W + nx2] == 0) {
              avalancheGrid[y * W + x] = 0;
              avalancheGrid[(y + 1) * W + nx2] = 1;
            }
          }
        }

        int occupied = 0;
        int occupiedTop = 0;
        for (int i = 0; i < W * H; i++) occupied += avalancheGrid[i] ? 1 : 0;
        for (int x = 0; x < W; x++) occupiedTop += avalancheGrid[x] ? 1 : 0;
        // A granular pile can trap tiny voids. Treat 98% coverage (or a
        // nearly sealed top edge) as visually full, invert foreground and
        // background, and let the next colour fall into place forever.
        if (occupied >= W * H * 98 / 100 || occupiedTop >= W * 15 / 16) {
          memset(avalancheGrid, 0, sizeof(avalancheGrid));
          avalancheWhitePhase = !avalancheWhitePhase;
        }
      }

      const float inverseHeight = 1.0f / H;
      for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
          const bool pixelIsWhite = avalancheWhitePhase
            ? avalancheGrid[y * W + x] != 0
            : avalancheGrid[y * W + x] == 0;
          if (pixelIsWhite) {
            const float palettePosition = (x * 0.35f + y * 0.65f) * inverseHeight +
                                          (avalancheWhitePhase ? 0.0f : 0.5f);
            put(x, y, paletteColorAt(palette, palettePosition));
          }
        }
      }
      break;
    }

    case BAYER_FLOW: {
      const BayerFlowParams& bdParams = *static_cast<const BayerFlowParams*>(controls);
      const float time = (enginePhase[0] += deltaTime * bdParams.as);
      // Tuned at 64 pixels; smaller panels show the same composition.
      const float frequency = (0.025f + bdParams.scale * 0.010f) * 64.0f / W;
      const float contrast = 0.70f + bdParams.contrast * 0.22f;
      const float driftX = time * 4.0f;
      const float driftY = time * 2.4f;
      const int baseX = (int)floorf(driftX);
      const int baseY = (int)floorf(driftY);
      const float fractionX = driftX - floorf(driftX);
      const float fractionY = driftY - floorf(driftY);
      float smoothThresholds[64];
      for (int by = 0; by < 8; by++) {
        for (int bx = 0; bx < 8; bx++) {
          const int sx0 = (bx + baseX) & 7;
          const int sy0 = (by + baseY) & 7;
          const int sx1 = (sx0 + 1) & 7;
          const int sy1 = (sy0 + 1) & 7;
          const float top = BAYER_8X8[sy0 * 8 + sx0] * (1.0f - fractionX) +
                            BAYER_8X8[sy0 * 8 + sx1] * fractionX;
          const float bottom = BAYER_8X8[sy1 * 8 + sx0] * (1.0f - fractionX) +
                               BAYER_8X8[sy1 * 8 + sx1] * fractionX;
          smoothThresholds[by * 8 + bx] =
            (top * (1.0f - fractionY) + bottom * fractionY + 0.5f) / 64.0f;
        }
      }

      const float diagonal = 0.48f / (W + H);
      for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
          float field = 0.50f +
            0.27f * fastSin(x * frequency + time * 1.13f) +
            0.23f * fastSin(y * frequency * 0.83f - time * 0.91f);
          field = constrain(0.5f + (field - 0.5f) * contrast, 0.0f, 1.0f);

          const float threshold = smoothThresholds[(y & 7) * 8 + (x & 7)];
          if (field > threshold) {
            const float palettePosition = field * 0.52f + (x + y) * diagonal + time * 0.025f;
            put(x, y, paletteColorAt(palette, palettePosition, 0.52f + field * 0.48f));
          }
        }
      }
      break;
    }

    case SNAKE: {
      const SnakeParams& snParams = *static_cast<const SnakeParams*>(controls);
      if (snakeLength < 1) resetSnake(snParams);
      snakeMoveAccumulator += deltaTime * max(1.0f, snParams.speed);
      int steps = 0;
      while (snakeMoveAccumulator >= 1.0f && steps++ < 6) {
        snakeMoveAccumulator -= 1.0f;
        if (!advanceSnake(snParams)) resetSnake(snParams);
      }

      for (int i = snakeLength - 1; i >= 0; i--) {
        const float position = snakeLength > 1
          ? 1.0f - (float)i / (snakeLength - 1)
          : 1.0f;
        // Tail to head runs body → highlight, so no segment drops to shadow.
        uint16_t color = paletteColorAt(palette, 0.25f + position * 0.3f);
        if (i == 0) color = rgb565(colorScale, colorScale, colorScale);  // A white head
        put(snakeX[i], snakeY[i], color);
      }
      put(snakeAppleX, snakeAppleY, rgb565(colorScale, 0, 0));  // and a red apple, in every palette.
      break;
    }

    case SQUARES: {
      const SquaresParams& sqParams = *static_cast<const SquaresParams*>(controls);
      const float time = (enginePhase[0] += deltaTime * sqParams.as);
      const int count = constrain(sqParams.count, 2, 20);
      const int thickness = constrain(sqParams.thickness, 1, 4);
      const float maximumHalfSize = fastDistance(cx, cy) + 3.0f;

      for (int i = 0; i < count; i++) {
        const float speedOffset = 1.0f + sqParams.variance * fastSin(i * 2.399f + 0.7f);
        squarePhase[i] = fmodf(squarePhase[i] + deltaTime * sqParams.as * 0.24f * speedOffset, 1.0f);
        float phase = fmodf(squarePhase[i] + i * sqParams.spacing / count, 1.0f);
        if (phase < 0.0f) phase += 1.0f;
        const float eased = phase * phase;
        // Squares centre on the panel: even-sized on an even panel.
        const int halfSize = 1 + (int)(eased * maximumHalfSize);
        const int left = (int)floorf(cx + 0.5f) - halfSize;
        const int top = (int)floorf(cy + 0.5f) - halfSize;
        const int right = (int)ceilf(cx - 0.5f) + halfSize;
        const int bottom = (int)ceilf(cy - 0.5f) + halfSize;
        const uint16_t color = paletteColorAt(palette,
          (float)i / count + time * 0.018f + phase * 0.18f);
        for (int line = 0; line < thickness && left + line <= right - line; line++) {
          const int x0 = left + line;
          const int y0 = top + line;
          const int x1 = right - line;
          const int y1 = bottom - line;
          for (int x = x0; x <= x1; x++) {
            put(x, y0, color);
            put(x, y1, color);
          }
          for (int y = y0 + 1; y < y1; y++) {
            put(x0, y, color);
            put(x1, y, color);
          }
        }
      }
      break;
    }

    case FLOW_SPHERE: {
      const FlowSphereParams& fsParams = *static_cast<const FlowSphereParams*>(controls);
      flowSphereTime += deltaTime * constrain(fsParams.speed, 0.05f, 6.0f);
      // Radius is in 64-pixel units, so the sphere fills every panel alike.
      const float radius = constrain(fsParams.radius, 14.0f, 31.0f) * W / 64.0f;
      const float inverseRadius = 1.0f / radius;
      const float scale = constrain(fsParams.scale, 0.5f, 4.0f);
      const float density = constrain(fsParams.density, 0.1f, 1.0f);
      // Filaments thin as density falls, down to a sparse few at its minimum.
      const float threshold = 0.5f + (0.8f - density) * (density < 0.8f ? 0.6f : 1.05f);
      const float rotation = flowSphereTime * 0.19f;
      const float sinRotation = fastSin(rotation);
      const float cosRotation = fastCos(rotation);

      for (int y = 0; y < H; y++) {
        const float ny = (y - cy) * inverseRadius;
        for (int x = 0; x < W; x++) {
          const float nx = (x - cx) * inverseRadius;
          const float radialSquared = nx * nx + ny * ny;
          if (radialSquared > 1.0f) continue;

          // Reconstruct the front hemisphere, then rotate the surface sample
          // through the flow field to make the sphere itself appear to turn.
          const float nz = sqrtf(max(0.0f, 1.0f - radialSquared));
          const float rx = nx * cosRotation + nz * sinRotation;
          const float rz = nz * cosRotation - nx * sinRotation;
          const float curlA = fastSin((rx * 4.1f + ny * 2.3f) * scale +
                                      flowSphereTime * 1.7f);
          const float curlB = fastSin((ny * 3.7f - rz * 2.9f) * scale -
                                      flowSphereTime * 1.31f + curlA * 1.8f);
          const float curlC = fastSin((rz * 4.6f + rx * 1.9f) * scale +
                                      flowSphereTime * 0.93f - curlB * 1.4f);
          const float ridge = fabsf(curlA * 0.48f + curlB * 0.34f +
                                    curlC * 0.28f);
          if (ridge < threshold) continue;

          const float edge = constrain(nz * 1.7f, 0.0f, 1.0f);
          const float light = constrain(0.30f + 0.52f * nz - 0.16f * nx,
                                        0.12f, 1.0f);
          const float palettePosition = 0.5f + 0.20f * curlA +
                                        0.17f * curlB + 0.13f * ny +
                                        flowSphereTime * 0.018f;
          put(x, y, paletteColorAt(palette, palettePosition, edge * light));
        }
      }
      break;
    }
  }
}
