/*
  64x64 HUB75 RGB Matrix Controller
  -----------------------------------
  Merged version:
  - Ports the original and new native patterns, LFO controls, three patch slots,
    default patches, auto-cycle, FPS and re-sync logic to a 64x64 HUB75
    RGB panel on the Adafruit MatrixPortal S3.
  - Adds saved multi-network Wi-Fi, automatic setup access point fallback,
    captive-portal DNS and a modern responsive web interface inspired by
    the second sketch.
  - Everything is contained in this one .ino file; no webpage.h is needed.

  Required libraries:
  - Adafruit Protomatter
  - Adafruit GFX Library
  - ArduinoJson
  - ESP32 Arduino core (WiFi, WebServer, ESPmDNS, Preferences, DNSServer)
*/

#if defined(MATRIX_PROFILE_WS2812)
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
#elif defined(MATRIX_HUB75_DMA)
#include "src/platform/DmaMatrix.h"
#else
#include <Adafruit_Protomatter.h>
#endif
#include <Adafruit_GFX.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <nvs.h>
#include <new>
#include <DNSServer.h>
#if defined(MATRIX_PROFILE_WS2812)
#include <LittleFS.h>
#endif
#ifndef MATRIX_ENABLE_OTA
#define MATRIX_ENABLE_OTA 1
#endif
#if MATRIX_ENABLE_OTA
#include <ArduinoOTA.h>
#endif
#include <math.h>
#include <esp_system.h>
#include <time.h>

#if defined(MATRIX_PROFILE_WS2812)
#include "src/platform/Ws2812Backend.h"
#else
#include "src/platform/ProtomatterBackend.h"
#endif
#include "src/patterns/HelloLuaPattern.h"
#include "src/patterns/LuaPatternRuntime.h"
#include "src/patterns/PatternflowPack.h"
#include "src/generated/PatternCatalog.h"
#include "src/generated/PatternPreviewAtlas.h"
#include "src/patterns/SoukanPalettes.h"

// -----------------------------------------------------------------------------
// Hardware profiles
// -----------------------------------------------------------------------------
#if defined(MATRIX_PROFILE_WS2812)
#ifndef WS2812_MATRIX_WIDTH
#define WS2812_MATRIX_WIDTH 16
#endif
#ifndef WS2812_MATRIX_HEIGHT
#define WS2812_MATRIX_HEIGHT 16
#endif
#define SCREEN_WIDTH    WS2812_MATRIX_WIDTH
#define SCREEN_HEIGHT   WS2812_MATRIX_HEIGHT
#define DISPLAY_SCALE   0.5f
#ifndef MATRIX_COLOR_DEPTH
#define MATRIX_COLOR_DEPTH 8
#endif
#ifndef MATRIX_LOW_LIGHT_DITHER
#define MATRIX_LOW_LIGHT_DITHER 0
#endif
#ifndef WS2812_DATA_PIN
#define WS2812_DATA_PIN 22
#endif
#ifndef WS2812_ROTATION
#define WS2812_ROTATION 0
#endif

#if WS2812_MATRIX_WIDTH == 22 && WS2812_MATRIX_HEIGHT == 22
const char* DEVICE_PROFILE_ID = "esp32-wroom-ws2812-22x22";
const char* DEVICE_DISPLAY_NAME = "Matrix.22";
const char* HOSTNAME = "matrix-22";
const char* SETUP_AP_SSID = "Matrix.22-Setup";
#else
const char* DEVICE_PROFILE_ID = "esp32-wroom-ws2812-16x16";
const char* DEVICE_DISPLAY_NAME = "Matrix.16";
const char* HOSTNAME = "matrix-16";
const char* SETUP_AP_SSID = "Matrix.16-Setup";
#endif
const char* DISPLAY_BACKEND_NAME = "ws2812";
const char* DISPLAY_REFRESH_LABEL = "WS2812 frame writes";

// Default mapping: data enters at the top-left, rows run horizontally, and
// alternate rows reverse direction. Override these flags for a differently
// wired panel without changing the pattern engine.
class PlatformNeoMatrix : public Adafruit_NeoMatrix {
 public:
  using Adafruit_NeoMatrix::Adafruit_NeoMatrix;
  uint16_t color565(uint8_t red, uint8_t green, uint8_t blue) const {
    return Color(red, green, blue);
  }
};

PlatformNeoMatrix matrix(
  SCREEN_WIDTH, SCREEN_HEIGHT, WS2812_DATA_PIN,
  NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG,
  NEO_GRB + NEO_KHZ800
);
Ws2812Backend displayBackend(matrix);

#else
#define SCREEN_WIDTH    64
#define SCREEN_HEIGHT   64
#define DISPLAY_SCALE   2.0f
#ifndef MATRIX_COLOR_DEPTH
#define MATRIX_COLOR_DEPTH 5
#endif
#ifndef MATRIX_LOW_LIGHT_DITHER
#define MATRIX_LOW_LIGHT_DITHER 0
#endif

const char* DEVICE_PROFILE_ID = "matrixportal-s3-hub75-64x64";
const char* DEVICE_DISPLAY_NAME = "Matrix.64";
#if defined(MATRIX_HUB75_DMA)
const char* DISPLAY_BACKEND_NAME = "hub75-dma-exact20-test";
#else
const char* DISPLAY_BACKEND_NAME = "hub75-protomatter";
#endif
const char* DISPLAY_REFRESH_LABEL = "HUB75 electrical refresh";
const char* HOSTNAME = "matrix-64";
const char* SETUP_AP_SSID = "Matrix.64-Setup";

// Official MatrixPortal ESP32-S3 HUB75 pin mapping.
uint8_t rgbPins[]  = {42, 41, 40, 38, 39, 37}; // R1,G1,B1,R2,G2,B2
uint8_t addrPins[] = {45, 36, 48, 35, 21};     // A,B,C,D,E
uint8_t clockPin   = 2;
uint8_t latchPin   = 47;
uint8_t oePin      = 14;

// Bit depth is selected per build profile. Double buffering prevents tearing;
// lower depths trade color precision for a higher electrical refresh rate.
#if defined(MATRIX_HUB75_DMA)
DmaMatrix matrix;
#else
Adafruit_Protomatter matrix(
  SCREEN_WIDTH, MATRIX_COLOR_DEPTH, 1, rgbPins,
  5, addrPins,
  clockPin, latchPin, oePin,
  true
);
#endif

ProtomatterBackend displayBackend(matrix);
#endif

LuaPatternRuntime luaPattern(displayBackend);
bool luaHelloEnabled = false;
bool depthComparisonEnabled = false;

// -----------------------------------------------------------------------------
// Wi-Fi configuration
// -----------------------------------------------------------------------------
// The normal way to add Wi-Fi is through the setup access point. These optional
// compile-time credentials can be filled in as a backup. Leave them empty to
// avoid storing a Wi-Fi password in source code.
const char* DEFAULT_WIFI_SSID = "";
const char* DEFAULT_WIFI_PASSWORD = "";

const char* SETUP_AP_PASSWORD = ""; // open setup access point

const byte DNS_PORT = 53;
const uint8_t MAX_WIFI_NETWORKS = 8;
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 7000;
const unsigned long WIFI_LOST_TO_AP_MS = 60000;

// -----------------------------------------------------------------------------
// Matrix, web server and global state
// -----------------------------------------------------------------------------
Preferences preferences;
bool patternBookmarks[GALLERY_PATTERN_COUNT] = {};
WebServer server(80);
DNSServer dnsServer;

volatile bool isUpdatingWeb = false;
bool isAPMode = false;
bool mdnsStarted = false;
bool otaStarted = false;
bool otaPreviousAutoCycle = false;
#if defined(MATRIX_PROFILE_WS2812)
bool patternStorageReady = false;
#endif
unsigned long wifiDisconnectedSince = 0;
unsigned long wifiRestartAt = 0;

int targetFPS = 60;
bool autoCycleEnabled = true;
unsigned long autoCycleIntervalSecs = 120;
unsigned long autoResyncInterval = 0; // HUB75/Protomatter: legacy MAX7219 re-sync disabled

bool scheduleEnabled = false;
uint16_t scheduleStartMinutes = 18 * 60;
uint16_t scheduleEndMinutes = 2 * 60;
uint16_t scheduleFadeInSeconds = 30;
uint16_t scheduleFadeOutSeconds = 5;
float scheduleOpacity = 1.0f;
float transitionOpacity = 1.0f;
float combinedOutputScale = 1.0f;
uint8_t autoCycleTransitionPhase = 0; // 0 idle, 1 fading out, 2 fading in
unsigned long autoCycleTransitionStartedMs = 0;
constexpr uint16_t AUTO_CYCLE_FADE_OUT_MS = 900;
constexpr uint16_t AUTO_CYCLE_FADE_IN_MS = 1600;
unsigned long lastFadeUpdateMs = 0;
#define NUM_PATCH_SLOTS 3
#define MAX_STARS 300

enum Animation {
  STARFIELD,
  WARPED_CHECKERBOARD,
  MOIRE_RINGS,
  MOIRE_LINES,
  WARPING_CHECKERS,
  SHOCKWAVE,
  WARP_CORRIDORS,
  PIXEL_AVALANCHE,
  KALEIDOSCOPE,
  BAYER_FLOW,
  FLOYD_FLOW,
  SNAKE,
  SQUARES,
  PERSPECTIVE_FLOW,
  FLOW_SPHERE,
  FLOW_FIELD_2D,
  PATTERNFLOW_COLLECTION
};

const int NUM_ANIMATIONS = 17;
const char* animationNames[NUM_ANIMATIONS] = {
  "Starfield", "Warped Checkerboard", "Moiré Rings", "Moiré Lines",
  "Warping Checkers", "Shockwave", "Warp Corridors", "Pixel Avalanche", "Kaleidoscope",
  "Bayer Flow", "Floyd Flow", "Snake", "Squares", "Perspective Flow",
  "Flow Sphere", "2D Flow Field", "Patternflow Collection"
};
const uint8_t animationModTargetCounts[NUM_ANIMATIONS] = {
  4, 4, 4, 4, 4, 3, 4, 2, 4, 4, 4, 3, 4, 4, 4, 5, 4
};

Animation currentAnimation = STARFIELD;
unsigned long lastAnimationChangeTime = 0;
unsigned long lastResyncTime = 0;
float animationTime = 0;

struct StarfieldParams          { int b, n; float as, s, w; int lfo_target; float lfo_speed, lfo_amount; };
struct WarpedCheckerboardParams { int b; float as, ws, d, wsp; int lfo_target; float lfo_speed, lfo_amount; };
struct MoireRingsParams         { int b; float as, orb, rt, es; int lfo_target; float lfo_speed, lfo_amount; };
struct MoireLinesParams         { int b; float as, rs1, rs2, lf; int lfo_target; float lfo_speed, lfo_amount; };
struct WarpingCheckersParams    { int b; float as, os, rt, es; int lfo_target; float lfo_speed, lfo_amount; };
struct ShockwaveParams          { int b; float as, ws, wt; int lfo_target; float lfo_speed, lfo_amount; };
struct WarpCorridorsParams      { int b; float as, rs, ws, bf; int lfo_target; float lfo_speed, lfo_amount; };
struct PixelAvalancheParams     { int b, sr; float as; int lfo_target; float lfo_speed, lfo_amount; };
struct KaleidoscopeParams       { int b, p; float as, rs, es; int lfo_target; float lfo_speed, lfo_amount; };
struct BayerFlowParams          { int b, scale, contrast; float as, drift; int lfo_target; float lfo_speed, lfo_amount; };
struct FloydFlowParams          { int b, levels, scale; float as, warp; int lfo_target; float lfo_speed, lfo_amount; };
struct SnakeParams              { int b, cell, length; float speed, steering; int lfo_target; float lfo_speed, lfo_amount; };
struct SquaresParams            { int b, count, thickness; float as, spacing, variance; int lfo_target; float lfo_speed, lfo_amount; };
struct PerspectiveFlowParams    { int b, detail; float density, amplitude, as; int lfo_target; float lfo_speed, lfo_amount; };
struct FlowSphereParams         { int b; float speed, scale, density, radius; int lfo_target; float lfo_speed, lfo_amount; };
struct FlowField2DParams        { int b, trails; float speed, scale, density, curl; int lfo_target; float lfo_speed, lfo_amount; };
struct PatternflowCollectionParams { int b, variant; uint8_t control[4]; int lfo_target; float lfo_speed, lfo_amount; };
struct ColorParams              { uint16_t hue, spread; uint8_t palette; };
struct ColorTuningParams        { int16_t hueShift; uint16_t saturation; };
struct PatternStyleParams       { float zoom, rotation; int8_t offsetX, offsetY, symmetry; };
StarfieldParams sfParams;
WarpedCheckerboardParams wcParams;
MoireRingsParams mrParams;
MoireLinesParams mlParams;
WarpingCheckersParams wchParams;
ShockwaveParams swParams;
WarpCorridorsParams wcoParams;
PixelAvalancheParams paParams;
KaleidoscopeParams kdParams;
BayerFlowParams bdParams;
FloydFlowParams fdParams;
SnakeParams snParams;
SquaresParams sqParams;
PerspectiveFlowParams pfParams;
FlowSphereParams fsParams;
FlowField2DParams f2Params;
PatternflowCollectionParams pfcParams;
PatternflowPack patternflowPack;
uint16_t patternflowLowRes[32 * 32] = {0};
// Native detail is the default. The optional fast mode reconstructs a smooth
// field with interpolation rather than repeating large square pixels.
uint8_t patternflowQuality[PATTERNFLOW_PATTERN_COUNT] = {};
const uint8_t patternflowRenderSize[PATTERNFLOW_PATTERN_COUNT] = {32,32,32,32,32,32,64,64,24,32,64,64,64,32,32,32,64,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,64,64,64,64,64,64};
ColorParams colorParams[NUM_ANIMATIONS];
ColorTuningParams colorTuning[NUM_ANIMATIONS];

StarfieldParams sfPatches[NUM_PATCH_SLOTS];
WarpedCheckerboardParams wcPatches[NUM_PATCH_SLOTS];
MoireRingsParams mrPatches[NUM_PATCH_SLOTS];
MoireLinesParams mlPatches[NUM_PATCH_SLOTS];
WarpingCheckersParams wchPatches[NUM_PATCH_SLOTS];
ShockwaveParams swPatches[NUM_PATCH_SLOTS];
WarpCorridorsParams wcoPatches[NUM_PATCH_SLOTS];
PixelAvalancheParams paPatches[NUM_PATCH_SLOTS];
KaleidoscopeParams kdPatches[NUM_PATCH_SLOTS];
BayerFlowParams bdPatches[NUM_PATCH_SLOTS];
FloydFlowParams fdPatches[NUM_PATCH_SLOTS];
SnakeParams snPatches[NUM_PATCH_SLOTS];
SquaresParams sqPatches[NUM_PATCH_SLOTS];
PerspectiveFlowParams pfPatches[NUM_PATCH_SLOTS];
FlowSphereParams fsPatches[NUM_PATCH_SLOTS];
FlowField2DParams f2Patches[NUM_PATCH_SLOTS];
PatternflowCollectionParams pfcPatches[NUM_PATCH_SLOTS];
ColorParams colorPatches[NUM_ANIMATIONS][NUM_PATCH_SLOTS];
ColorTuningParams colorTuningPatches[NUM_ANIMATIONS][NUM_PATCH_SLOTS];
PatternStyleParams styleParams[NUM_ANIMATIONS];
PatternStyleParams activeStyleParams = {1.0f, 0.0f, 0, 0, 0};
PatternStyleParams stylePatches[NUM_ANIMATIONS][NUM_PATCH_SLOTS];
uint32_t customPalette[NUM_ANIMATIONS][4];
uint32_t customPalettePatches[NUM_ANIMATIONS][NUM_PATCH_SLOTS][4];
uint32_t paletteStops[NUM_ANIMATIONS][Soukan::Total][4];
uint32_t paletteStopPatches[NUM_ANIMATIONS][NUM_PATCH_SLOTS][Soukan::Total][4];
float activeStyleCosine = 1.0f;
float activeStyleSine = 0.0f;

int defaultPatches[NUM_ANIMATIONS] = {0};

struct Star { float x, y, z; };
Star stars[MAX_STARS];
float distMap[SCREEN_HEIGHT][SCREEN_WIDTH];
byte avalancheGrid[SCREEN_HEIGHT][SCREEN_WIDTH] = {0};
float avalancheTimeSinceUpdate = 0;
bool avalancheWhitePhase = true;
float flowSphereTime = 0.0f;
float flowField2DTime = 0.0f;
constexpr int MAX_FLOW_FIELD_PARTICLES = 512;
struct FlowFieldParticle { float x, y; uint16_t seed; };
FlowFieldParticle flowFieldParticles[MAX_FLOW_FIELD_PARTICLES];
bool flowField2DInitialized = false;
constexpr int MAX_SNAKE_SEGMENTS = 512;
int8_t snakeX[MAX_SNAKE_SEGMENTS] = {0};
int8_t snakeY[MAX_SNAKE_SEGMENTS] = {0};
int snakeLength = 0;
int snakeDirection = 0;
int snakeAppleX = 0;
int snakeAppleY = 0;
int snakeCellSize = 0;
float snakeMoveAccumulator = 0.0f;
int16_t diffusionErrorA[SCREEN_WIDTH + 2] = {0};
int16_t diffusionErrorB[SCREEN_WIDTH + 2] = {0};
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

// Fast rendering helpers. Full-screen effects call their inner loop thousands
// of times per frame, so transcendental math and HSV conversion must stay out
// of that loop. These small internal-RAM tables are faster than PSRAM and also
// fit comfortably on the future ESP32-WROOM targets.
constexpr uint16_t SIN_LUT_SIZE = 1024;
constexpr uint16_t SIN_LUT_MASK = SIN_LUT_SIZE - 1;
constexpr float RADIANS_TO_SIN_INDEX = SIN_LUT_SIZE / (2.0f * PI);
float sinLut[SIN_LUT_SIZE];
uint16_t huePalette[256];
int huePaletteBrightness = -1;
int huePaletteAnimation = -1;
uint8_t huePaletteSaturation = 0;
uint8_t huePaletteValue = 0;
ColorParams huePaletteColorParams = {0, 0, 255};
ColorTuningParams huePaletteTuningParams = {0, 100};

unsigned long lastFrameTime = 0;
unsigned long nextFrameDueTime = 0;

struct PerformanceMetrics {
  float actualFPS = 0.0f;
  float panelRefreshHz = 0.0f;
  uint32_t averageRenderUs = 0;
  uint32_t averageDrawUs = 0;
  uint32_t averageShowUs = 0;
  uint32_t worstRenderUs = 0;
  uint32_t lastRenderUs = 0;
  uint32_t overBudgetFrames = 0;
  uint32_t sampleFrames = 0;
  uint32_t windowFrames = 0;
  uint32_t windowOverBudgetFrames = 0;
  uint32_t windowWorstRenderUs = 0;
  uint64_t windowRenderUs = 0;
  uint64_t windowDrawUs = 0;
  uint64_t windowShowUs = 0;
  unsigned long windowStartedAtMs = 0;
  unsigned long lastSerialLogAtMs = 0;
};

PerformanceMetrics performance;
uint32_t stateRevision = 1;
// Shadow of the final styled RGB565 pixels. It is used only by the development
// sampler to create gallery previews from the real firmware renderer.
uint16_t previewCapture[SCREEN_WIDTH * SCREEN_HEIGHT] = {0};

// Shared USB serial command buffer. 128 bytes accommodates the longest valid
// Wi-Fi provisioning command (32-byte SSID + 64-byte password) as well as the
// USB dashboard commands.
char usbSerialLine[1024] = {0};
uint8_t usbSerialLength = 0;

// -----------------------------------------------------------------------------
// Forward declarations
// -----------------------------------------------------------------------------
void setupWebServer();
void setupOta();
void applyCurrentBrightness();
void loadPatch(Animation anim, int patchIndex);
void advanceAutoCyclePattern();
void setMatrixPoint(int y, int x, bool state);
void setMatrixPointColor(int y, int x, uint16_t color);
uint16_t makeColor(float hue, float saturation, float value, int brightness);
uint16_t makeWhite(float value, int brightness);
float fastSin(float radians);
float fastDistance(float dx, float dy);
void ensureHuePalette(uint8_t saturation, uint8_t value, int brightness);
uint16_t huePaletteColor(float hue);
uint16_t scaleRgb565(uint16_t color, uint8_t scale);
uint16_t makePaletteColor(float position, uint8_t saturation, uint8_t value,
                          int brightness, const ColorParams& controls);
void resyncDisplay();
void updatePlaybackFades();
void loadSettings();
void startAccessPoint();
bool connectToKnownWifi();
bool addOrUpdateNetwork(const String& ssid, const String& password);
bool forceSetupAccessPoint();
void setForceSetupAccessPoint(bool enabled);
void maintainWifi();
void handleRoot();
void handlePreviewAtlas();
void handleSetupPage();
void handleControlGet();
void handleControlPost();
void handleDiagnosticsGet();
void handleWifiAdd();
void handleWifiReset();
void handleWifiScan();
void handleSetupSave();
void handleNotFound();
void recordRenderedFrame(uint32_t drawUs, uint32_t showUs,
                         uint32_t frameBudgetUs);
void resetSnake(int cellSize);
void initializeStyleDefaults();
void renderDepthComparison(float timeSeconds);
void processUsbSerial();
void printPreviewFrame();
String buildControlJson();
String buildDiagnosticsJson();
String buildWifiScanJson();
String processControlCommandJson(const String& body, int& status);

#include "src/patterns/ColorCalibration.inc"
#include "src/patterns/LibrarySettings.inc"
#include "src/patterns/LibraryManagement.inc"

void parseUsbSerialLine(const char* line) {
  if (!strcmp(line, "USBGET,LIBRARY")) {
    Serial.print("USBJSON,LIBRARY,");Serial.println(buildLibraryJson());return;
  }
  if (!strcmp(line, "USBGET,CONTROL")) {
    Serial.print("USBJSON,CONTROL,");
    Serial.println(buildControlJson());
    return;
  }

  if (!strcmp(line, "USBGET,DIAGNOSTICS")) {
    Serial.print("USBJSON,DIAGNOSTICS,");
    Serial.println(buildDiagnosticsJson());
    return;
  }

  if (!strcmp(line, "USBGET,WIFISCAN")) {
    Serial.print("USBJSON,WIFISCAN,");
    Serial.println(buildWifiScanJson());
    return;
  }

  if (!strcmp(line, "USBGET,PREVIEW")) {
    printPreviewFrame();
    return;
  }

  if (!strncmp(line, "USBPOST,CONTROL,", 16)) {
    int status = 500;
    const String result = processControlCommandJson(String(line + 16), status);
    Serial.printf("USBJSON,RESULT,%d,%s\n", status, result.c_str());
    return;
  }

  if (!strcmp(line, "WIFI,AP")) {
    setForceSetupAccessPoint(true);
    Serial.println("WIFI,AP,restarting");
    Serial.flush();
    delay(150);
    ESP.restart();
    return;
  }

  if (!strcmp(line, "WIFI,RESET")) {
    preferences.begin("wifi-list", false);
    preferences.clear();
    preferences.end();
    Serial.println("WIFI,RESET,restarting");
    Serial.flush();
    delay(150);
    ESP.restart();
    return;
  }

  if (!strcmp(line, "WIFI,STATUS")) {
    Serial.printf("WIFI,STATUS,%d,%s,%s\n", (int)WiFi.status(),
                  WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    return;
  }

  if (!strcmp(line, "WIFI,SCAN")) {
    const int found = WiFi.scanNetworks(false, true);
    Serial.printf("WIFI,SCAN,%d\n", found);
    for (int i = 0; i < found; i++) {
      Serial.printf("WIFI,NETWORK,%s,%d,%d,%d\n", WiFi.SSID(i).c_str(),
                    (int)WiFi.channel(i), (int)WiFi.RSSI(i),
                    (int)WiFi.encryptionType(i));
    }
    WiFi.scanDelete();
    return;
  }

  // USB provisioning intentionally stores credentials only in ESP32 NVS. They
  // are never echoed and never become part of the compiled firmware image.
  // Format: WIFI,<ssid>,<password>
  if (!strncmp(line, "WIFI,", 5)) {
    const char* separator = strchr(line + 5, ',');
    if (!separator) {
      Serial.println("WIFI,ERROR,expected WIFI,<ssid>,<password>");
      return;
    }

    String ssid(line + 5);
    ssid.remove((unsigned int)(separator - (line + 5)));
    ssid.trim();
    const String password(separator + 1);
    if (!addOrUpdateNetwork(ssid, password)) {
      Serial.println("WIFI,ERROR,could not save network");
      return;
    }

    Serial.println("WIFI,SAVED,restarting");
    Serial.flush();
    delay(150);
    ESP.restart();
    return;
  }

  unsigned long pingSequence = 0;
  char pingTrailing = 0;
  if (sscanf(line, "PING,%lu%c", &pingSequence, &pingTrailing) == 1) {
    Serial.printf("PONG,%lu\n", pingSequence);
    return;
  }

}

void processUsbSerial() {
  while (Serial.available() > 0) {
    const char incoming = (char)Serial.read();
    if (incoming == '\n' || incoming == '\r') {
      if (usbSerialLength > 0) {
        usbSerialLine[usbSerialLength] = '\0';
        parseUsbSerialLine(usbSerialLine);
        usbSerialLength = 0;
      }
      continue;
    }
    if (usbSerialLength < sizeof(usbSerialLine) - 1) {
      usbSerialLine[usbSerialLength++] = incoming;
    } else {
      usbSerialLength = 0;
    }
  }
}

bool snakeContains(int x, int y, int count = -1) {
  const int limit = count < 0 ? snakeLength : min(count, snakeLength);
  for (int i = 0; i < limit; i++) {
    if (snakeX[i] == x && snakeY[i] == y) return true;
  }
  return false;
}

void placeSnakeApple() {
  const int gridWidth = SCREEN_WIDTH / snakeCellSize;
  const int gridHeight = SCREEN_HEIGHT / snakeCellSize;
  for (int attempt = 0; attempt < 256; attempt++) {
    const int x = random(gridWidth);
    const int y = random(gridHeight);
    if (!snakeContains(x, y)) {
      snakeAppleX = x;
      snakeAppleY = y;
      return;
    }
  }
  snakeAppleX = 0;
  snakeAppleY = 0;
}

void resetSnake(int cellSize) {
  snakeCellSize = constrain(cellSize, 1, 4);
  const int gridWidth = SCREEN_WIDTH / snakeCellSize;
  const int gridHeight = SCREEN_HEIGHT / snakeCellSize;
  snakeLength = min(constrain(snParams.length, 4, MAX_SNAKE_SEGMENTS),
                    min(12, gridWidth - 2));
  snakeLength = max(4, snakeLength);
  snakeDirection = 0;
  snakeMoveAccumulator = 0.0f;
  const int headX = max(snakeLength + 1, gridWidth / 2);
  const int headY = gridHeight / 2;
  for (int i = 0; i < snakeLength; i++) {
    snakeX[i] = headX - i;
    snakeY[i] = headY;
  }
  placeSnakeApple();
}

bool advanceSnake(const SnakeParams& params) {
  static const int8_t dx[4] = {1, 0, -1, 0};
  static const int8_t dy[4] = {0, 1, 0, -1};
  const int gridWidth = SCREEN_WIDTH / snakeCellSize;
  const int gridHeight = SCREEN_HEIGHT / snakeCellSize;
  int safeDirections[3];
  int safeCount = 0;
  int bestDirection = -1;
  int bestDistance = 10000;

  for (int direction = 0; direction < 4; direction++) {
    if (direction == ((snakeDirection + 2) & 3)) continue;
    const int nx = snakeX[0] + dx[direction];
    const int ny = snakeY[0] + dy[direction];
    if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
    if (snakeContains(nx, ny, max(0, snakeLength - 1))) continue;
    safeDirections[safeCount++] = direction;
    const int distance = abs(nx - snakeAppleX) + abs(ny - snakeAppleY);
    if (distance < bestDistance) {
      bestDistance = distance;
      bestDirection = direction;
    }
  }

  if (safeCount == 0) return false;
  const bool steerTowardApple = random(1000) < (int)(params.steering * 1000.0f);
  snakeDirection = steerTowardApple && bestDirection >= 0
    ? bestDirection
    : safeDirections[random(safeCount)];

  const int newX = snakeX[0] + dx[snakeDirection];
  const int newY = snakeY[0] + dy[snakeDirection];
  const bool ateApple = newX == snakeAppleX && newY == snakeAppleY;
  const int maximumLength = min(min(params.length, MAX_SNAKE_SEGMENTS),
                                gridWidth * gridHeight - 1);
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

void renderInstallingUpdateFrame() {
  matrix.fillScreen(0);
  memset(previewCapture, 0, sizeof(previewCapture));
#if defined(MATRIX_PROFILE_WS2812)
  const uint16_t border = matrix.color565(32, 16, 0);
  const uint16_t activity = matrix.color565(0, 32, 32);
  matrix.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, border);
  matrix.drawLine(3, SCREEN_HEIGHT / 2, SCREEN_WIDTH - 5,
                  SCREEN_HEIGHT / 2, activity);
  matrix.drawLine(SCREEN_WIDTH - 7, SCREEN_HEIGHT / 2 - 3,
                  SCREEN_WIDTH - 4, SCREEN_HEIGHT / 2, activity);
  matrix.drawLine(SCREEN_WIDTH - 7, SCREEN_HEIGHT / 2 + 3,
                  SCREEN_WIDTH - 4, SCREEN_HEIGHT / 2, activity);
#else
  matrix.setTextWrap(false);
  matrix.setTextSize(1);
  matrix.setTextColor(matrix.color565(0, 32, 32));
  matrix.setCursor(2, 25);
  matrix.print("INSTALLING");
  matrix.setCursor(11, 36);
  matrix.print("UPDATE");
#endif
  displayBackend.show();
}

void setupOta() {
#if MATRIX_ENABLE_OTA
  if (otaStarted || WiFi.status() != WL_CONNECTED) return;
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setRebootOnSuccess(true);
  ArduinoOTA.onStart([]() {
    isUpdatingWeb = true;
    otaPreviousAutoCycle = autoCycleEnabled;
    autoCycleEnabled = false;
    renderInstallingUpdateFrame();
#if !defined(MATRIX_PROFILE_WS2812)
    // Flash writes pause normal interrupt service and can freeze an enabled
    // HUB75 row. Briefly show the dim notice before writes begin, then let
    // Protomatter disable output and its timer for the entire transfer.
    delay(250);
    matrix.stop();
#endif
    Serial.println("OTA update started");
  });
  ArduinoOTA.onEnd([]() { Serial.println("OTA update complete; rebooting"); });
  ArduinoOTA.onError([](ota_error_t error) {
#if !defined(MATRIX_PROFILE_WS2812)
    matrix.resume();
#endif
    autoCycleEnabled = otaPreviousAutoCycle;
    isUpdatingWeb = false;
    Serial.printf("OTA update failed: %u\n", (unsigned)error);
  });
  ArduinoOTA.begin();
  otaStarted = true;
  Serial.println("Arduino OTA receiver ready");
#endif
}

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------
void setup() {
  preferences.begin("matrix-v2", true);
  bool legacyBookmarks[64] = {};
  const size_t legacyBookmarkBytes=preferences.getBytesLength("bookmarks");
  if(legacyBookmarkBytes && legacyBookmarkBytes<=sizeof(legacyBookmarks))
    preferences.getBytes("bookmarks",legacyBookmarks,legacyBookmarkBytes);
  for(int i=0;i<GALLERY_PATTERN_COUNT;i++)patternBookmarks[i]=legacyBookmarks[bookmarkStorageIndex(i)];
  preferences.getBytes("quality", patternflowQuality, sizeof(patternflowQuality));
  preferences.end();
  // v2.0 removed all audio routing. Its legacy blob can fill the small NVS
  // partition and prevent new flags from saving. Remove only that unused key;
  // never erase the namespace or any pattern/network settings.
  Preferences obsoleteAudio;
  if(obsoleteAudio.begin("matrix-cfg",false)){
    if(obsoleteAudio.isKey("audio_p"))obsoleteAudio.remove("audio_p");
    obsoleteAudio.end();
  }
  // Compact flags share the existing namespace: one NVS entry per bitmap.
  Preferences flags;
  if(flags.begin("matrix-cfg",true)){
    if(flags.isKey("qbits")){
      const uint64_t bits=flags.getULong64("qbits",0);
      const uint64_t upper=flags.getULong64("qbits_hi",0);
      for(int i=0;i<PATTERNFLOW_PATTERN_COUNT;i++)patternflowQuality[i]=((i<64?bits:upper)>>(i%64))&1;
    }
    if(flags.isKey("bmarks")){
      const uint64_t bits=flags.getULong64("bmarks",0);
      for(int i=0;i<GALLERY_PATTERN_COUNT;i++)patternBookmarks[i]=(bits>>bookmarkStorageIndex(i))&1;
    }
    flags.end();
  }
  Serial.begin(115200);
  delay(100);
  Serial.printf("\n\nESP32 Matrix Platform — %s\n", DEVICE_DISPLAY_NAME);

  // Network credentials are managed explicitly in the wifi-list Preferences
  // namespace. Keep the ESP-IDF Wi-Fi driver configuration in RAM so it does
  // not duplicate AP/STA settings into the already busy NVS partition.
  WiFi.persistent(false);

#if defined(MATRIX_PROFILE_WS2812)
  matrix.begin();
  matrix.setRotation(WS2812_ROTATION);
  // Keep the pixel buffer at full precision; pattern brightness and scheduled
  // fades are applied by the shared rendering pipeline.
  matrix.setBrightness(255);
  Serial.printf("WS2812 ready: %dx%d, GPIO %d, rotation %d\n",
                SCREEN_WIDTH, SCREEN_HEIGHT, WS2812_DATA_PIN,
                WS2812_ROTATION);
  patternStorageReady = LittleFS.begin(false);
  if (patternStorageReady) {
    Serial.printf("Pattern storage ready: %u / %u bytes used\n",
                  (unsigned)LittleFS.usedBytes(),
                  (unsigned)LittleFS.totalBytes());
  } else {
    Serial.println("Pattern storage not mounted (upload/format it before installing files).");
  }
#else
#if defined(MATRIX_HUB75_DMA)
  bool matrixReady=matrix.begin();
  Serial.printf("DMA exact-20-MHz begin: %s\n",matrixReady?"OK":"FAILED");
  if(!matrixReady){while(true)delay(100);}
#else
  ProtomatterStatus matrixStatus = matrix.begin();
  Serial.print("Protomatter begin() status: ");
  Serial.println((int)matrixStatus);
  if (matrixStatus != PROTOMATTER_OK) {
    Serial.println("ERROR: HUB75 matrix initialization failed.");
    while (true) delay(100);
  }
#endif
#endif
  matrix.fillScreen(0);
  displayBackend.show();

  if (luaPattern.loadFromString(HELLO_LUA_PATTERN, "hello-lua")) {
    Serial.print("Lua runtime ready. Pattern memory: ");
    Serial.print(luaPattern.memoryUsed());
    Serial.print(" / ");
    Serial.println(luaPattern.memoryLimit());
  } else {
    Serial.print("Lua runtime unavailable: ");
    Serial.println(luaPattern.lastError());
  }

  loadSettings();
  configureCuratedLibrary();
  loadColorCalibration();
  loadLibraryMetadata();
  if(isPatternArchived(currentAnimation,pfcParams.variant))advanceAutoCyclePattern();
  applyCurrentBrightness();

  randomSeed((uint32_t)esp_random());

  for (uint16_t i = 0; i < SIN_LUT_SIZE; i++) {
    sinLut[i] = sinf(i * (2.0f * PI / SIN_LUT_SIZE));
  }

  const float centerX = SCREEN_WIDTH / 2.0f - 0.5f;
  const float centerY = SCREEN_HEIGHT / 2.0f - 0.5f;
  for (int y = 0; y < SCREEN_HEIGHT; y++) {
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      distMap[y][x] = sqrtf((x - centerX) * (x - centerX) + (y - centerY) * (y - centerY));
    }
  }

  for (int i = 0; i < MAX_STARS; i++) {
    stars[i] = {
      random(-SCREEN_WIDTH, SCREEN_WIDTH) * 1.0f,
      random(-SCREEN_HEIGHT, SCREEN_HEIGHT) * 1.0f,
      random(1, SCREEN_WIDTH) * 1.0f
    };
  }

  setupWebServer();

  if (forceSetupAccessPoint()) {
    Serial.println("Forced Wi-Fi setup mode requested.");
    startAccessPoint();
  } else if (!connectToKnownWifi()) {
    startAccessPoint();
  }

  server.begin();
  Serial.println("HTTP server started");

  setupOta();

  lastAnimationChangeTime = millis();
  lastResyncTime = millis();
  lastFrameTime = micros();
  nextFrameDueTime = lastFrameTime;
}

// -----------------------------------------------------------------------------
// Main loop — animation functionality retained from the first sketch
// -----------------------------------------------------------------------------
void recordRenderedFrame(uint32_t drawUs, uint32_t showUs,
                         uint32_t frameBudgetUs) {
  const unsigned long nowMs = millis();
  if (performance.windowStartedAtMs == 0) {
    performance.windowStartedAtMs = nowMs;
#if defined(MATRIX_HUB75_DMA)
    matrix.getFrameCount(); // Exclude startup/network-connection scans.
#endif
  }

  const uint32_t renderUs = drawUs + showUs;
  performance.lastRenderUs = renderUs;
  performance.windowFrames++;
  performance.windowRenderUs += renderUs;
  performance.windowDrawUs += drawUs;
  performance.windowShowUs += showUs;
  performance.windowWorstRenderUs = max(performance.windowWorstRenderUs, renderUs);
  if (renderUs > frameBudgetUs) performance.windowOverBudgetFrames++;

  const unsigned long elapsedMs = nowMs - performance.windowStartedAtMs;
  if (elapsedMs < 1000) return;

  performance.actualFPS = performance.windowFrames * 1000.0f / elapsedMs;
#if defined(MATRIX_PROFILE_WS2812)
  // Addressable pixels retain the last transmitted frame; there is no
  // independent HUB75 scan refresh. Report physical bus writes instead.
  performance.panelRefreshHz = performance.actualFPS;
#else
  performance.panelRefreshHz = matrix.getFrameCount() * 1000.0f / elapsedMs;
#endif
  performance.averageRenderUs = performance.windowFrames > 0
    ? (uint32_t)(performance.windowRenderUs / performance.windowFrames)
    : 0;
  performance.averageDrawUs = performance.windowFrames > 0
    ? (uint32_t)(performance.windowDrawUs / performance.windowFrames)
    : 0;
  performance.averageShowUs = performance.windowFrames > 0
    ? (uint32_t)(performance.windowShowUs / performance.windowFrames)
    : 0;
  performance.worstRenderUs = performance.windowWorstRenderUs;
  performance.overBudgetFrames = performance.windowOverBudgetFrames;
  performance.sampleFrames = performance.windowFrames;

  performance.windowFrames = 0;
  performance.windowRenderUs = 0;
  performance.windowDrawUs = 0;
  performance.windowShowUs = 0;
  performance.windowWorstRenderUs = 0;
  performance.windowOverBudgetFrames = 0;
  performance.windowStartedAtMs = nowMs;

  if (nowMs - performance.lastSerialLogAtMs >= 5000) {
    const char* patternName = depthComparisonEnabled
      ? "3-bit / 4-bit Test"
      : (luaHelloEnabled ? "Hello Lua" : animationNames[(int)currentAnimation]);
    Serial.printf(
      "PERF pattern=%s fps=%.1f/%d panel_refresh=%.1fHz draw=%.2fms show=%.2fms render_avg=%.2fms render_worst=%.2fms over_budget=%lu/%lu heap=%lu min_heap=%lu\n",
      patternName, performance.actualFPS, targetFPS, performance.panelRefreshHz,
      performance.averageDrawUs / 1000.0f,
      performance.averageShowUs / 1000.0f,
      performance.averageRenderUs / 1000.0f,
      performance.worstRenderUs / 1000.0f,
      (unsigned long)performance.overBudgetFrames,
      (unsigned long)performance.sampleFrames,
      (unsigned long)ESP.getFreeHeap(),
      (unsigned long)ESP.getMinFreeHeap());
    performance.lastSerialLogAtMs = nowMs;
  }
}

bool scheduleShouldBeOn() {
  if (!scheduleEnabled) return true;
  struct tm localTime;
  if (!getLocalTime(&localTime, 0)) return true;
  const uint16_t minute = localTime.tm_hour * 60 + localTime.tm_min;
  if (scheduleStartMinutes == scheduleEndMinutes) return true;
  if (scheduleStartMinutes < scheduleEndMinutes) {
    return minute >= scheduleStartMinutes && minute < scheduleEndMinutes;
  }
  return minute >= scheduleStartMinutes || minute < scheduleEndMinutes;
}

void advanceAutoCyclePattern() {
  luaHelloEnabled=false;
  int next=LibraryMeta::nextActive(galleryIndex(currentAnimation,pfcParams.variant),GALLERY_PATTERN_COUNT,isGalleryArchived);
  if(next<0)return; // Empty active library: output is black; dashboard stays usable.
  if(next<CORE_PATTERN_COUNT){currentAnimation=(Animation)CORE_PATTERN_IDS[next];loadPatch(currentAnimation,defaultPatches[currentAnimation]);}
  else{currentAnimation=PATTERNFLOW_COLLECTION;int v=COLLECTION_PATTERN_IDS[next-CORE_PATTERN_COUNT];loadVariantPatch(v,variantDefaults[v]);}
}

void updatePlaybackFades() {
  const unsigned long now = millis();
  const float elapsedSeconds = lastFadeUpdateMs == 0
    ? 0.0f
    : min((now - lastFadeUpdateMs) / 1000.0f, 0.25f);
  lastFadeUpdateMs = now;

  const bool shouldBeOn = scheduleShouldBeOn();
  const float duration = max(0.1f, (shouldBeOn
    ? scheduleFadeInSeconds
    : scheduleFadeOutSeconds) * 1.0f);
  const float step = elapsedSeconds / duration;
  scheduleOpacity = constrain(scheduleOpacity + (shouldBeOn ? step : -step),
                              0.0f, 1.0f);

  if (!isPatternArchived(currentAnimation,pfcParams.variant) && !calibrationMode && !depthComparisonEnabled && autoCycleTransitionPhase == 0 && autoCycleEnabled &&
      now - lastAnimationChangeTime >= autoCycleIntervalSecs * 1000UL) {
    autoCycleTransitionPhase = 1;
    autoCycleTransitionStartedMs = now;
  }

  if (autoCycleTransitionPhase == 1) {
    const float progress = constrain(
      (now - autoCycleTransitionStartedMs) / (float)AUTO_CYCLE_FADE_OUT_MS,
      0.0f, 1.0f);
    transitionOpacity = 1.0f - progress;
    if (progress >= 1.0f) {
      advanceAutoCyclePattern();
      lastAnimationChangeTime = now;
      autoCycleTransitionPhase = 2;
      autoCycleTransitionStartedMs = now;
      stateRevision++;
    }
  } else if (autoCycleTransitionPhase == 2) {
    const float progress = constrain(
      (now - autoCycleTransitionStartedMs) / (float)AUTO_CYCLE_FADE_IN_MS,
      0.0f, 1.0f);
    transitionOpacity = progress;
    if (progress >= 1.0f) {
      transitionOpacity = 1.0f;
      autoCycleTransitionPhase = 0;
    }
  } else {
    transitionOpacity = 1.0f;
  }

  combinedOutputScale = scheduleOpacity * transitionOpacity;
  if(isPatternArchived(currentAnimation,pfcParams.variant)&&!calibrationMode&&!depthComparisonEnabled&&!luaHelloEnabled)combinedOutputScale=0.f;
  displayBackend.setOutputScale(combinedOutputScale);
}

void loop() {
  processUsbSerial();
#if MATRIX_ENABLE_OTA
  if (otaStarted) ArduinoOTA.handle();
#endif
  server.handleClient();
  if (wifiRestartAt && int32_t(millis()-wifiRestartAt)>=0) ESP.restart();
  if (isAPMode) dnsServer.processNextRequest();
  maintainWifi();

  if (autoResyncInterval > 0 &&
      (millis() - lastResyncTime >= autoResyncInterval * 1000UL)) {
    resyncDisplay();
    lastResyncTime = millis();
  }

  updatePlaybackFades();

  if(calibrationMode && millis()-calibrationStarted>600000UL){calibrationMode=0;lastAnimationChangeTime=millis();stateRevision++;}

  if (isUpdatingWeb) return;

  const unsigned long currentTime = micros();
  const unsigned long frameDuration =
    (targetFPS > 0) ? (1000000UL / (unsigned long)targetFPS) : 1000000UL;

  if ((int32_t)(currentTime - nextFrameDueTime) < 0) return;

  // Keep a stable deadline instead of restarting the timer from a slightly
  // late frame. This prevents small Wi-Fi/web scheduling delays from lowering
  // the average FPS. Skip stale deadlines after a larger interruption.
  if ((unsigned long)(currentTime - nextFrameDueTime) > frameDuration) {
    nextFrameDueTime = currentTime;
  }
  nextFrameDueTime += frameDuration;

  float deltaTime = (currentTime - lastFrameTime) / 1000000.0f;
  if (deltaTime > 0.12f) deltaTime = 0.12f; // avoid giant simulation jumps
  lastFrameTime = currentTime;
  const float scaledDeltaTime = deltaTime;
  animationTime += scaledDeltaTime;
  const unsigned long renderStartedAt = micros();

  if (calibrationMode) {
    renderColorCalibration();
    const unsigned long showStartedAt=micros();displayBackend.show();
    recordRenderedFrame(showStartedAt-renderStartedAt,micros()-showStartedAt,frameDuration);
    return;
  }

  if (depthComparisonEnabled) {
    renderDepthComparison(animationTime);
    const unsigned long showStartedAt = micros();
    displayBackend.show();
    recordRenderedFrame(showStartedAt - renderStartedAt,
                        micros() - showStartedAt, frameDuration);
    return;
  }

  if (luaHelloEnabled) {
    if (luaPattern.frame(animationTime, scaledDeltaTime)) {
      const unsigned long showStartedAt = micros();
      displayBackend.show();
      recordRenderedFrame(showStartedAt - renderStartedAt,
                          micros() - showStartedAt, frameDuration);
      return;
    }

    luaHelloEnabled = false;
    Serial.print("Hello Lua stopped: ");
    Serial.println(luaPattern.lastError());
  }

  matrix.fillScreen(0);
  memset(previewCapture, 0, sizeof(previewCapture));
  activeStyleParams = styleParams[(int)currentAnimation];
  const float styleAngle = activeStyleParams.rotation *
                           (PI / 180.0f);
  activeStyleSine = fastSin(styleAngle);
  activeStyleCosine = fastSin(styleAngle + PI * 0.5f);

  // --- LFO Modulation Stage -------------------------------------------------
  StarfieldParams modSf = sfParams;
  WarpedCheckerboardParams modWc = wcParams;
  MoireRingsParams modMr = mrParams;
  MoireLinesParams modMl = mlParams;
  WarpingCheckersParams modWch = wchParams;
  ShockwaveParams modSw = swParams;
  WarpCorridorsParams modWco = wcoParams;
  PixelAvalancheParams modPa = paParams;
  KaleidoscopeParams modKd = kdParams;
  BayerFlowParams modBd = bdParams;
  FloydFlowParams modFd = fdParams;
  SnakeParams modSn = snParams;
  SquaresParams modSq = sqParams;
  PerspectiveFlowParams modPf = pfParams;
  FlowSphereParams modFs = fsParams;
  FlowField2DParams modF2 = f2Params;
  PatternflowCollectionParams modPfc = pfcParams;

  switch (currentAnimation) {
    case STARFIELD:
      if (sfParams.lfo_target > 0 && sfParams.lfo_amount > 0) {
        float lfoRaw = (sinf(animationTime * sfParams.lfo_speed) + 1.0f) * 0.5f;
        float lfo = 0.5f + (lfoRaw - 0.5f) * sfParams.lfo_amount;
        switch (sfParams.lfo_target) {
          case 1: modSf.as = 0.1f + 2.9f * lfo; break;
          case 2: modSf.n = 10 + (int)(290.0f * lfo); break;
          case 3: modSf.s = 2.0f + 8.0f * lfo; break;
          case 4: modSf.w = 0.05f + 0.95f * lfo; break;
        }
      }
      break;

    case WARPED_CHECKERBOARD:
      if (wcParams.lfo_target > 0 && wcParams.lfo_amount > 0) {
        float lfoRaw = (sinf(animationTime * wcParams.lfo_speed) + 1.0f) * 0.5f;
        float lfo = 0.5f + (lfoRaw - 0.5f) * wcParams.lfo_amount;
        switch (wcParams.lfo_target) {
          case 1: modWc.as = 0.1f + 2.9f * lfo; break;
          case 2: modWc.ws = 2.0f + 8.0f * lfo; break;
          case 3: modWc.d = 1.0f + 7.0f * lfo; break;
          case 4: modWc.wsp = 1.0f + 7.0f * lfo; break;
        }
      }
      break;

    case MOIRE_RINGS:
      if (mrParams.lfo_target > 0 && mrParams.lfo_amount > 0) {
        float lfoRaw = (sinf(animationTime * mrParams.lfo_speed) + 1.0f) * 0.5f;
        float lfo = 0.5f + (lfoRaw - 0.5f) * mrParams.lfo_amount;
        switch (mrParams.lfo_target) {
          case 1: modMr.as = 0.1f + 2.9f * lfo; break;
          case 2: modMr.orb = 16.0f * lfo; break;
          case 3: modMr.rt = 2.0f + 14.0f * lfo; break;
          case 4: modMr.es = 0.1f + 4.9f * lfo; break;
        }
      }
      break;

    case MOIRE_LINES:
      if (mlParams.lfo_target > 0 && mlParams.lfo_amount > 0) {
        float lfoRaw = (sinf(animationTime * mlParams.lfo_speed) + 1.0f) * 0.5f;
        float lfo = 0.5f + (lfoRaw - 0.5f) * mlParams.lfo_amount;
        switch (mlParams.lfo_target) {
          case 1: modMl.as = 0.1f + 2.9f * lfo; break;
          case 2: modMl.rs1 = 0.1f + 2.9f * lfo; break;
          case 3: modMl.rs2 = 0.1f + 2.9f * lfo; break;
          case 4: modMl.lf = 0.1f + 1.9f * lfo; break;
        }
      }
      break;

    case WARPING_CHECKERS:
      if (wchParams.lfo_target > 0 && wchParams.lfo_amount > 0) {
        float lfoRaw = (sinf(animationTime * wchParams.lfo_speed) + 1.0f) * 0.5f;
        float lfo = 0.5f + (lfoRaw - 0.5f) * wchParams.lfo_amount;
        switch (wchParams.lfo_target) {
          case 1: modWch.as = 0.1f + 2.9f * lfo; break;
          case 2: modWch.os = 0.1f + 1.9f * lfo; break;
          case 3: modWch.rt = 1.0f + 7.0f * lfo; break;
          case 4: modWch.es = 0.1f + 1.9f * lfo; break;
        }
      }
      break;

    case SHOCKWAVE:
      if (swParams.lfo_target > 0 && swParams.lfo_amount > 0) {
        float lfoRaw = (sinf(animationTime * swParams.lfo_speed) + 1.0f) * 0.5f;
        float lfo = 0.5f + (lfoRaw - 0.5f) * swParams.lfo_amount;
        switch (swParams.lfo_target) {
          case 1: modSw.as = 0.1f + 2.9f * lfo; break;
          case 2: modSw.ws = 0.1f + 4.9f * lfo; break;
          case 3: modSw.wt = 0.1f + 4.9f * lfo; break;
        }
      }
      break;

    case WARP_CORRIDORS:
      if (wcoParams.lfo_target > 0 && wcoParams.lfo_amount > 0) {
        float lfoRaw = (sinf(animationTime * wcoParams.lfo_speed) + 1.0f) * 0.5f;
        float lfo = 0.5f + (lfoRaw - 0.5f) * wcoParams.lfo_amount;
        switch (wcoParams.lfo_target) {
          case 1: modWco.as = 0.1f + 2.9f * lfo; break;
          case 2: modWco.rs = 0.1f + 1.9f * lfo; break;
          case 3: modWco.ws = 0.1f + 1.9f * lfo; break;
          case 4: modWco.bf = 0.1f + 1.9f * lfo; break;
        }
      }
      break;

    case PIXEL_AVALANCHE:
      if (paParams.lfo_target > 0 && paParams.lfo_amount > 0) {
        float lfoRaw = (sinf(animationTime * paParams.lfo_speed) + 1.0f) * 0.5f;
        float lfo = 0.5f + (lfoRaw - 0.5f) * paParams.lfo_amount;
        switch (paParams.lfo_target) {
          case 1: modPa.as = 0.1f + 2.9f * lfo; break;
          case 2: modPa.sr = 1 + (int)(9.0f * lfo); break;
        }
      }
      break;

    case KALEIDOSCOPE:
      if (kdParams.lfo_target > 0 && kdParams.lfo_amount > 0) {
        float lfoRaw = (sinf(animationTime * kdParams.lfo_speed) + 1.0f) * 0.5f;
        float lfo = 0.5f + (lfoRaw - 0.5f) * kdParams.lfo_amount;
        switch (kdParams.lfo_target) {
          case 1: modKd.as = 0.1f + 2.9f * lfo; break;
          case 2: modKd.p = 1 + (int)(14.0f * lfo); break;
          case 3: modKd.rs = 0.1f + 1.9f * lfo; break;
          case 4: modKd.es = 0.1f + 1.9f * lfo; break;
        }
      }
      break;

    case BAYER_FLOW:
      if (bdParams.lfo_target > 0 && bdParams.lfo_amount > 0) {
        float lfoRaw = (sinf(animationTime * bdParams.lfo_speed) + 1.0f) * 0.5f;
        float lfo = 0.5f + (lfoRaw - 0.5f) * bdParams.lfo_amount;
        switch (bdParams.lfo_target) {
          case 1: modBd.as = 0.1f + 2.9f * lfo; break;
          case 2: modBd.scale = 1 + (int)(15.0f * lfo); break;
          case 3: modBd.contrast = 1 + (int)(7.0f * lfo); break;
          case 4: modBd.drift = 0.1f + 2.9f * lfo; break;
        }
      }
      break;

    case FLOYD_FLOW:
      if (fdParams.lfo_target > 0 && fdParams.lfo_amount > 0) {
        float lfoRaw = (sinf(animationTime * fdParams.lfo_speed) + 1.0f) * 0.5f;
        float lfo = 0.5f + (lfoRaw - 0.5f) * fdParams.lfo_amount;
        switch (fdParams.lfo_target) {
          case 1: modFd.as = 0.1f + 2.9f * lfo; break;
          case 2: modFd.levels = 2 + (int)(4.0f * lfo); break;
          case 3: modFd.scale = 1 + (int)(15.0f * lfo); break;
          case 4: modFd.warp = 0.1f + 2.9f * lfo; break;
        }
      }
      break;

    case SNAKE:
      if (snParams.lfo_target > 0 && snParams.lfo_amount > 0) {
        const float lfoRaw = (sinf(animationTime * snParams.lfo_speed) + 1.0f) * 0.5f;
        const float lfo = 0.5f + (lfoRaw - 0.5f) * snParams.lfo_amount;
        switch (snParams.lfo_target) {
          case 1: modSn.speed = 1.0f + 19.0f * lfo; break;
          case 2: modSn.length = 8 + (int)(292.0f * lfo); break;
          case 3: modSn.steering = lfo; break;
        }
      }
      break;

    case SQUARES:
      if (sqParams.lfo_target > 0 && sqParams.lfo_amount > 0) {
        const float lfoRaw = (sinf(animationTime * sqParams.lfo_speed) + 1.0f) * 0.5f;
        const float lfo = 0.5f + (lfoRaw - 0.5f) * sqParams.lfo_amount;
        switch (sqParams.lfo_target) {
          case 1: modSq.as = 0.1f + 3.9f * lfo; break;
          case 2: modSq.count = 2 + (int)(18.0f * lfo); break;
          case 3: modSq.spacing = 0.2f + 1.8f * lfo; break;
          case 4: modSq.variance = 0.5f * lfo; break;
        }
      }
      break;

    case PERSPECTIVE_FLOW:
      if (pfParams.lfo_target > 0 && pfParams.lfo_amount > 0) {
        const float lfoRaw = (sinf(animationTime * pfParams.lfo_speed) + 1.0f) * 0.5f;
        const float lfo = 0.5f + (lfoRaw - 0.5f) * pfParams.lfo_amount;
        switch (pfParams.lfo_target) {
          case 1: modPf.density = 2.3f + 2.7f * lfo; break;
          case 2: modPf.amplitude = 15.0f + 45.0f * lfo; break;
          case 3: modPf.as = 0.1f + 4.9f * lfo; break;
          case 4: modPf.detail = 3 + (int)(9.0f * lfo); break;
        }
      }
      break;

    case FLOW_SPHERE:
      if (fsParams.lfo_target > 0 && fsParams.lfo_amount > 0) {
        const float lfoRaw = (sinf(animationTime * fsParams.lfo_speed) + 1.0f) * 0.5f;
        const float lfo = 0.5f + (lfoRaw - 0.5f) * fsParams.lfo_amount;
        switch (fsParams.lfo_target) {
          case 1: modFs.speed = 0.05f + 5.95f * lfo; break;
          case 2: modFs.scale = 0.5f + 3.5f * lfo; break;
          case 3: modFs.density = 0.1f + 0.9f * lfo; break;
          case 4: modFs.radius = 14.0f + 17.0f * lfo; break;
        }
      }
      break;

    case FLOW_FIELD_2D:
      if (f2Params.lfo_target > 0 && f2Params.lfo_amount > 0) {
        const float lfoRaw = (sinf(animationTime * f2Params.lfo_speed) + 1.0f) * 0.5f;
        const float lfo = 0.5f + (lfoRaw - 0.5f) * f2Params.lfo_amount;
        switch (f2Params.lfo_target) {
          case 1: modF2.speed = 0.05f + 7.95f * lfo; break;
          case 2: modF2.scale = 0.3f + 3.7f * lfo; break;
          case 3: modF2.density = 0.05f + 0.95f * lfo; break;
          case 4: modF2.curl = 0.1f + 2.9f * lfo; break;
          case 5: modF2.trails = 1 + (int)(7.0f * lfo); break;
        }
      }
      break;

    case PATTERNFLOW_COLLECTION:
      if (pfcParams.lfo_target > 0 && pfcParams.lfo_amount > 0) {
        const float raw = (sinf(animationTime * pfcParams.lfo_speed) + 1.0f) * 0.5f;
        const float lfo = 0.5f + (raw - 0.5f) * pfcParams.lfo_amount;
        const int control = constrain(pfcParams.lfo_target - 1, 0, 3);
        modPfc.control[control] = (uint8_t)roundf(lfo * 100.0f);
      }
      break;
  }

  const float cx = (SCREEN_WIDTH - 1) * 0.5f;
  const float cy = (SCREEN_HEIGHT - 1) * 0.5f;

  // --- 64x64 RGB animation engine ------------------------------------------
  // All patterns below are self-contained and always render a non-zero set of
  // pixels with valid default parameters. This avoids depending on artifacts
  // from the old 32x32/MAX7219 coordinate mapping.
  switch (currentAnimation) {
    case STARFIELD: {
      const int starCount = constrain(modSf.n, 1, MAX_STARS);
      ensureHuePalette(107, 255, modSf.b);
      for (int i = 0; i < starCount; i++) {
        stars[i].z -= modSf.w * 50.0f * scaledDeltaTime * modSf.as;
        if (stars[i].z < 1.0f) {
          stars[i] = {
            random(-SCREEN_WIDTH, SCREEN_WIDTH) * 1.0f,
            random(-SCREEN_HEIGHT, SCREEN_HEIGHT) * 1.0f,
            random(1, SCREEN_WIDTH) * 1.0f
          };
        }

        int sx = (int)((stars[i].x / stars[i].z) *
                       (SCREEN_WIDTH / modSf.s) + cx);
        int sy = (int)((stars[i].y / stars[i].z) *
                       (SCREEN_HEIGHT / modSf.s) + cy);

        float nearFactor = 1.0f - constrain(stars[i].z / SCREEN_WIDTH, 0.0f, 1.0f);
        float hue = 0.56f + i * 0.007f + animationTime * 0.015f;
        setMatrixPointColor(sy, sx,
          scaleRgb565(huePaletteColor(hue),
                      (uint8_t)((0.35f + 0.65f * nearFactor) * 255.0f)));
      }
      break;
    }

    case WARPED_CHECKERBOARD: {
      const float time = animationTime * modWc.as;
      const float waveScale = max(1.0f, modWc.ws * DISPLAY_SCALE);
      const float distortion = modWc.d * DISPLAY_SCALE;
      ensureHuePalette(128, 199, modWc.b);

      for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
          const float warp = fastSin(distMap[y][x] / waveScale - time * modWc.wsp) * distortion;
          const bool on = (((x + y + (int)floorf(warp)) & 1) == 0);
          if (on) {
            const float hue = 0.69f + 0.055f * fastSin(time * 0.22f + distMap[y][x] * 0.075f);
            setMatrixPointColor(y, x, huePaletteColor(hue));
          }
        }
      }
      break;
    }

    case MOIRE_LINES: {
      const float time = animationTime * modMl.as;
      const float angle1 = time * modMl.rs1;
      const float angle2 = -time * modMl.rs2;
      const float c1 = cosf(angle1), s1 = sinf(angle1);
      const float c2 = cosf(angle2), s2 = sinf(angle2);
      const float spatial = max(0.04f, modMl.lf * 0.42f);
      ensureHuePalette(107, 194, modMl.b);

      for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
          const float px = x - cx;
          const float py = y - cy;
          const float v1 = px * c1 + py * s1;
          const float v2 = px * c2 + py * s2;
          const bool wave1 = fastSin(v1 * spatial + time * 0.18f) > 0.0f;
          const bool wave2 = fastSin(v2 * spatial - time * 0.15f) > 0.0f;

          if (wave1 != wave2) {
            const float hue = 0.57f + (v1 - v2) * 0.0035f + time * 0.010f;
            setMatrixPointColor(y, x, huePaletteColor(hue));
          }
        }
      }
      break;
    }

    case SHOCKWAVE: {
      const float time = animationTime * modSw.as;
      const float wavelength = max(0.5f, modSw.wt * DISPLAY_SCALE);
      ensureHuePalette(97, 255, modSw.b);
      for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
          const float wave = fastSin(distMap[y][x] / wavelength - time * modSw.ws);
          const int phase = (wave > 0.0f) ? 1 : 0;
          if (((x + y) & 1) == phase) {
            const float hue = 0.02f + 0.08f * (wave + 1.0f) + time * 0.006f;
            const uint8_t valueScale = (uint8_t)((0.58f + fabsf(wave) * 0.24f) * 255.0f);
            setMatrixPointColor(y, x, scaleRgb565(huePaletteColor(hue), valueScale));
          }
        }
      }
      break;
    }

    case WARP_CORRIDORS: {
      const float time = animationTime * modWco.as;
      const float angle = time * modWco.rs;
      const float sinA = sinf(angle), cosA = cosf(angle);
      const float breathing = 1.3f + 0.7f * sinf(time * modWco.ws);
      const float spatial = max(0.015f, 0.20f * modWco.bf * breathing);
      const float phase = time * 2.4f;
      ensureHuePalette(122, 255, modWco.b);

      for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
          const float px = x - cx;
          const float py = y - cy;
          const float rotX = px * cosA - py * sinA;
          const float rotY = px * sinA + py * cosA;
          const float perspective = 1.0f + fabsf(rotY) * 0.012f;
          const float wave = fastSin(rotX * spatial * perspective + phase);

          if (wave > 0.0f) {
            const float hue = 0.51f + rotY * 0.0025f + time * 0.010f;
            const uint8_t valueScale = (uint8_t)((0.62f + 0.22f * wave) * 255.0f);
            setMatrixPointColor(y, x, scaleRgb565(huePaletteColor(hue), valueScale));
          }
        }
      }
      break;
    }

    case PIXEL_AVALANCHE: {
      avalancheTimeSinceUpdate += scaledDeltaTime;
      const float updateInterval = 1.0f / max(1.0f, 18.0f * modPa.as);
      ensureHuePalette(220, 255, modPa.b);

      while (avalancheTimeSinceUpdate >= updateInterval) {
        avalancheTimeSinceUpdate -= updateInterval;

        for (int i = 0; i < modPa.sr; i++) {
          const int spawnX = random(SCREEN_WIDTH);
          if (avalancheGrid[0][spawnX] == 0) avalancheGrid[0][spawnX] = 1;
        }

        for (int y = SCREEN_HEIGHT - 2; y >= 0; y--) {
          for (int x = 0; x < SCREEN_WIDTH; x++) {
            if (avalancheGrid[y][x] == 0) continue;

            if (avalancheGrid[y + 1][x] == 0) {
              avalancheGrid[y][x] = 0;
              avalancheGrid[y + 1][x] = 1;
              continue;
            }

            const int dir = random(2) ? 1 : -1;
            const int nx1 = x + dir;
            const int nx2 = x - dir;
            if (nx1 >= 0 && nx1 < SCREEN_WIDTH && avalancheGrid[y + 1][nx1] == 0) {
              avalancheGrid[y][x] = 0;
              avalancheGrid[y + 1][nx1] = 1;
            } else if (nx2 >= 0 && nx2 < SCREEN_WIDTH && avalancheGrid[y + 1][nx2] == 0) {
              avalancheGrid[y][x] = 0;
              avalancheGrid[y + 1][nx2] = 1;
            }
          }
        }

        int occupied = 0;
        int occupiedTop = 0;
        for (int y = 0; y < SCREEN_HEIGHT; y++) {
          for (int x = 0; x < SCREEN_WIDTH; x++) occupied += avalancheGrid[y][x] ? 1 : 0;
        }
        for (int x = 0; x < SCREEN_WIDTH; x++) occupiedTop += avalancheGrid[0][x] ? 1 : 0;
        // A granular pile can trap tiny voids. Treat 98% coverage (or a
        // nearly sealed top edge) as visually full, invert foreground and
        // background, and let the next color fall into place forever.
        if (occupied >= SCREEN_WIDTH * SCREEN_HEIGHT * 98 / 100 ||
            occupiedTop >= SCREEN_WIDTH * 15 / 16) {
          memset(avalancheGrid, 0, sizeof(avalancheGrid));
          avalancheWhitePhase = !avalancheWhitePhase;
        }
      }

      for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
          const bool pixelIsWhite = avalancheWhitePhase
            ? avalancheGrid[y][x] != 0
            : avalancheGrid[y][x] == 0;
          if (pixelIsWhite) {
            const float palettePosition = (x * 0.35f + y * 0.65f) /
                                          SCREEN_HEIGHT +
                                          (avalancheWhitePhase ? 0.0f : 0.5f);
            setMatrixPointColor(y, x,
                                huePaletteColor(palettePosition));
          }
        }
      }
      break;
    }

    case BAYER_FLOW: {
      const float time = animationTime * modBd.as;
      const float frequency = 0.025f + modBd.scale * 0.010f;
      const float contrast = 0.70f + modBd.contrast * 0.22f;
      const float driftX = time * modBd.drift * 5.0f;
      const float driftY = time * modBd.drift * 3.0f;
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
      ensureHuePalette(220, 255, modBd.b);

      for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
          float field = 0.50f +
            0.27f * fastSin(x * frequency + time * 1.13f) +
            0.23f * fastSin(y * frequency * 0.83f - time * 0.91f);
          field = constrain(0.5f + (field - 0.5f) * contrast, 0.0f, 1.0f);

          const float threshold = smoothThresholds[(y & 7) * 8 + (x & 7)];
          if (field > threshold) {
            const float palettePosition = field * 0.52f +
              (x + y) * (0.48f / (SCREEN_WIDTH + SCREEN_HEIGHT)) + time * 0.025f;
            const uint8_t intensity = (uint8_t)((0.52f + field * 0.48f) * 255.0f);
            setMatrixPointColor(y, x,
              scaleRgb565(huePaletteColor(palettePosition), intensity));
          }
        }
      }
      break;
    }

    case SNAKE: {
      const int cellSize = 1;
      if (snakeCellSize != cellSize || snakeLength < 1) resetSnake(cellSize);
      snakeMoveAccumulator += scaledDeltaTime * max(1.0f, modSn.speed);
      int steps = 0;
      while (snakeMoveAccumulator >= 1.0f && steps++ < 6) {
        snakeMoveAccumulator -= 1.0f;
        if (!advanceSnake(modSn)) resetSnake(cellSize);
      }

      ensureHuePalette(190, 255, modSn.b);
      for (int i = snakeLength - 1; i >= 0; i--) {
        const float position = snakeLength > 1
          ? 1.0f - (float)i / (snakeLength - 1)
          : 1.0f;
        const bool pigment=Soukan::active(colorParams[SNAKE].palette);
        uint16_t color = huePaletteColor(pigment?.5f+position*.035f:0.08f+position*.72f);
        if (i == 0) color = pigment?huePaletteColor(.25f):makeWhite(1.0f, modSn.b);
        setMatrixPointColor(snakeY[i], snakeX[i], color);
      }
      setMatrixPointColor(snakeAppleY, snakeAppleX,
                          Soukan::active(colorParams[SNAKE].palette)?huePaletteColor(0.f):makeColor(0.0f, 1.0f, 1.0f, modSn.b));
      break;
    }

    case SQUARES: {
      const float time = animationTime * modSq.as;
      const int count = constrain(modSq.count, 2, 20);
      const int thickness = constrain(modSq.thickness, 1, 4);
      const float maximumHalfSize = fastDistance(cx, cy) + 3.0f;
      ensureHuePalette(220, 255, modSq.b);

      for (int i = 0; i < count; i++) {
        const float speedOffset = 1.0f + modSq.variance *
          fastSin(i * 2.399f + 0.7f);
        float phase = fmodf(time * 0.24f * speedOffset +
                            i * modSq.spacing / count, 1.0f);
        if (phase < 0.0f) phase += 1.0f;
        const float eased = phase * phase;
        const int halfSize = 1 + (int)(eased * maximumHalfSize);
        const int left = (int)roundf(cx) - halfSize;
        const int top = (int)roundf(cy) - halfSize;
        const int size = halfSize * 2 + 1;
        const uint16_t color = huePaletteColor(
          (float)i / count + time * 0.018f + phase * 0.18f);
        for (int line = 0; line < thickness && size - line * 2 > 0; line++) {
          const int x0 = left + line;
          const int y0 = top + line;
          const int x1 = left + size - 1 - line;
          const int y1 = top + size - 1 - line;
          for (int x = x0; x <= x1; x++) {
            setMatrixPointColor(y0, x, color);
            setMatrixPointColor(y1, x, color);
          }
          for (int y = y0 + 1; y < y1; y++) {
            setMatrixPointColor(y, x0, color);
            setMatrixPointColor(y, x1, color);
          }
        }
      }
      break;
    }

    case FLOW_SPHERE: {
      // A 2D LED interpretation of the user's Unity AudioFlowfield setup:
      // an evolving multi-axis noise-like field is evaluated on a projected
      // sphere. The default patch routes kick and snare envelopes to speed,
      // so transients push the field forward while it drifts slowly at idle.
      flowSphereTime += scaledDeltaTime * constrain(modFs.speed, 0.05f, 6.0f);
      const float radius = constrain(modFs.radius, 14.0f, 31.0f);
      const float inverseRadius = 1.0f / radius;
      const float scale = constrain(modFs.scale, 0.5f, 4.0f);
      const float density = constrain(modFs.density, 0.1f, 1.0f);
      const float threshold = 1.34f - density * 1.05f;
      const float rotation = flowSphereTime * 0.19f;
      const float sinRotation = fastSin(rotation);
      const float cosRotation = fastSin(rotation + PI * 0.5f);
      ensureHuePalette(255, 255, modFs.b);

      for (int y = 0; y < SCREEN_HEIGHT; y++) {
        const float ny = (y - cy) * inverseRadius;
        for (int x = 0; x < SCREEN_WIDTH; x++) {
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
          const uint8_t intensity = (uint8_t)(255.0f * edge * light);
          const float palettePosition = 0.5f + 0.20f * curlA +
                                        0.17f * curlB + 0.13f * ny +
                                        flowSphereTime * 0.018f;
          setMatrixPointColor(y, x,
            scaleRgb565(huePaletteColor(palettePosition), intensity));
        }
      }
      break;
    }

    case PATTERNFLOW_COLLECTION: {
      ensureHuePalette(255, 255, modPfc.b);
      ensureImportedRgbPalette(modPfc.b);
      const uint8_t variant = constrain(modPfc.variant, 0,
                                        PATTERNFLOW_PATTERN_COUNT - 1);
      const uint8_t desiredSize=patternflowQuality[variant] == 0 ? 64 : patternflowRenderSize[variant];
      const uint8_t renderSize=min((int)desiredSize,
                                   min(SCREEN_WIDTH,SCREEN_HEIGHT));
#if defined(MATRIX_PROFILE_WS2812)
        const bool direct=false;
#else
        const bool direct=activeStyleParams.zoom==1.f && activeStyleParams.rotation==0.f &&
          activeStyleParams.offsetX==0 && activeStyleParams.offsetY==0 && activeStyleParams.symmetry==0 &&
          combinedOutputScale>=.999f && !MATRIX_LOW_LIGHT_DITHER;
#endif
      if(renderSize>=min(SCREEN_WIDTH,SCREEN_HEIGHT)){
        if(direct){
          patternflowPack.render(variant,SCREEN_WIDTH,SCREEN_HEIGHT,scaledDeltaTime,
            modPfc.control,huePalette,importedRgbPalette,
            [](void*,int16_t x,int16_t y,uint16_t color){
              if(x>=0 && y>=0 && x<SCREEN_WIDTH && y<SCREEN_HEIGHT)
                previewCapture[y*SCREEN_WIDTH+x]=color;
            },nullptr);
#if !defined(MATRIX_PROFILE_WS2812)
          memcpy(matrix.getBuffer(),previewCapture,sizeof(previewCapture));
#endif
        }else{
        patternflowPack.render(
          variant, SCREEN_WIDTH, SCREEN_HEIGHT, scaledDeltaTime,
          modPfc.control, huePalette, importedRgbPalette,
          [](void*, int16_t x, int16_t y, uint16_t color) {
            setMatrixPointColor(y, x, color);
          },nullptr);
        }
      }else{
        struct LowResolutionContext{uint16_t* pixels;uint8_t width,height;};
        LowResolutionContext low={patternflowLowRes,renderSize,renderSize};
        memset(patternflowLowRes,0,sizeof(patternflowLowRes));
        patternflowPack.render(
          variant,renderSize,renderSize,scaledDeltaTime,modPfc.control,huePalette,importedRgbPalette,
          [](void* raw,int16_t x,int16_t y,uint16_t color){
            auto* target=static_cast<LowResolutionContext*>(raw);
            if(x>=0&&y>=0&&x<target->width&&y<target->height)
              target->pixels[y*target->width+x]=color;
          },&low);
        uint8_t x0[SCREEN_WIDTH],x1[SCREEN_WIDTH],wx[SCREEN_WIDTH];
        uint8_t y0[SCREEN_HEIGHT],y1[SCREEN_HEIGHT],wy[SCREEN_HEIGHT];
        for(int x=0;x<SCREEN_WIDTH;x++){
          const int source=constrain(((2*x+1)*renderSize*128/SCREEN_WIDTH)-128,0,(renderSize-1)*256);
          x0[x]=source>>8;x1[x]=min(x0[x]+1,renderSize-1);wx[x]=((source&255)+4)>>3;
        }
        for(int y=0;y<SCREEN_HEIGHT;y++){
          const int source=constrain(((2*y+1)*renderSize*128/SCREEN_HEIGHT)-128,0,(renderSize-1)*256);
          y0[y]=source>>8;y1[y]=min(y0[y]+1,renderSize-1);wy[y]=((source&255)+4)>>3;
        }
        for(int y=0;y<SCREEN_HEIGHT;y++)for(int x=0;x<SCREEN_WIDTH;x++){
          const uint16_t a=patternflowLowRes[y0[y]*renderSize+x0[x]],b=patternflowLowRes[y0[y]*renderSize+x1[x]];
          const uint16_t c=patternflowLowRes[y1[y]*renderSize+x0[x]],d=patternflowLowRes[y1[y]*renderSize+x1[x]];
          // Separate green from red/blue so all three channels interpolate
          // together with integer arithmetic. Five-bit weights cannot carry
          // across the channel gaps. No spatial or temporal dithering.
          auto packed=[](uint16_t color){
            return (uint32_t(color)|(uint32_t(color)<<16))&0x07E0F81FU;
          };
          const uint32_t top=((packed(a)*(32-wx[x])+packed(b)*wx[x])>>5)&0x07E0F81FU;
          const uint32_t bottom=((packed(c)*(32-wx[x])+packed(d)*wx[x])>>5)&0x07E0F81FU;
          const uint32_t blended=((top*(32-wy[y])+bottom*wy[y])>>5)&0x07E0F81FU;
          const uint16_t color=uint16_t(blended|(blended>>16));
          if(direct)previewCapture[y*SCREEN_WIDTH+x]=color;
          else if(color)setMatrixPointColor(y,x,color);
        }
#if !defined(MATRIX_PROFILE_WS2812)
        if(direct)memcpy(matrix.getBuffer(),previewCapture,sizeof(previewCapture));
#endif
      }
      break;
    }
  }

  const unsigned long showStartedAt = micros();
  displayBackend.show();
  recordRenderedFrame(showStartedAt - renderStartedAt,
                      micros() - showStartedAt, frameDuration);
}

// -----------------------------------------------------------------------------
// Persistence and matrix utilities
// -----------------------------------------------------------------------------
void initializeStyleDefaults() {
  static const uint32_t defaultColors[4] = {
    0xFF3155, 0xFFC247, 0x22D3C5, 0x765DFF
  };
  const auto& defaultPaletteStops=LIBRARY_PALETTES;
  for (int animation = 0; animation < NUM_ANIMATIONS; animation++) {
    styleParams[animation] = {1.0f, 0.0f, 0, 0, 0};
    colorTuning[animation] = {0, 100};
    memcpy(customPalette[animation], defaultColors, sizeof(defaultColors));
    memcpy(paletteStops[animation], defaultPaletteStops,
           sizeof(defaultPaletteStops));
    for(int c=0;c<4;c++)paletteStops[animation][7][c]=compiledStop(animation,0,7,c);
    for (int patch = 0; patch < NUM_PATCH_SLOTS; patch++) {
      stylePatches[animation][patch] = styleParams[animation];
      colorTuningPatches[animation][patch] = colorTuning[animation];
      memcpy(customPalettePatches[animation][patch], defaultColors,
             sizeof(defaultColors));
      memcpy(paletteStopPatches[animation][patch],paletteStops[animation],sizeof(paletteStops[animation]));
    }
  }

  const uint32_t avalancheWhite[4] = {
    0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF
  };
  const uint32_t perspectiveGray[4] = {
    0x181818, 0x686868, 0xFFFFFF, 0xA0A0A0
  };
  memcpy(customPalette[PIXEL_AVALANCHE], avalancheWhite,
         sizeof(avalancheWhite));
  memcpy(customPalette[PERSPECTIVE_FLOW], perspectiveGray,
         sizeof(perspectiveGray));
  memcpy(paletteStops[PIXEL_AVALANCHE][6], avalancheWhite,
         sizeof(avalancheWhite));
  memcpy(paletteStops[PERSPECTIVE_FLOW][6], perspectiveGray,
         sizeof(perspectiveGray));
  for (int patch = 0; patch < NUM_PATCH_SLOTS; patch++) {
    memcpy(customPalettePatches[PIXEL_AVALANCHE][patch], avalancheWhite,
           sizeof(avalancheWhite));
    memcpy(customPalettePatches[PERSPECTIVE_FLOW][patch], perspectiveGray,
           sizeof(perspectiveGray));
    memcpy(paletteStopPatches[PIXEL_AVALANCHE][patch][6], avalancheWhite,
           sizeof(avalancheWhite));
    memcpy(paletteStopPatches[PERSPECTIVE_FLOW][patch][6], perspectiveGray,
           sizeof(perspectiveGray));
  }
}

void initializeDefaultPatches() {
  initializeStyleDefaults();
  sfParams  = {5, 120, 1.0f, 4.0f, 0.2f, 0, 0.5f, 1.0f};
  wcParams  = {6, 1.0f, 5.0f, 4.0f, 3.0f, 0, 0.5f, 1.0f};
  mrParams  = {6, 1.0f, 14.0f, 8.0f, 0.2f, 0, 0.5f, 1.0f};
  mlParams  = {6, 1.0f, 1.5f, 1.2f, 0.6f, 0, 0.5f, 1.0f};
  wchParams = {6, 1.0f, 1.0f, 3.0f, 0.5f, 0, 0.5f, 1.0f};
  swParams  = {5, 1.0f, 2.5f, 2.0f, 0, 0.5f, 1.0f};
  wcoParams = {6, 1.0f, 0.25f, 0.6f, 1.0f, 0, 0.5f, 1.0f};
  paParams  = {7, 4, 1.0f, 0, 0.5f, 1.0f};
  kdParams  = {7, 7, 1.0f, 1.0f, 1.0f, 0, 0.5f, 1.0f};
  bdParams  = {7, 7, 4, 1.0f, 0.8f, 0, 0.5f, 1.0f};
  fdParams  = {7, 4, 7, 1.0f, 1.0f, 0, 0.5f, 1.0f};
  snParams  = {7, 1, 96, 8.0f, 0.82f, 0, 0.5f, 1.0f};
  sqParams  = {7, 10, 1, 1.0f, 1.0f, 0.16f, 0, 0.5f, 1.0f};
  pfParams  = {7, 6, 3.0f, 28.0f, 1.0f, 0, 0.5f, 1.0f};
  fsParams  = {7, 0.12f, 1.45f, 0.56f, 28.0f, 0, 0.5f, 1.0f};
  f2Params  = {7, 4, 0.16f, 1.25f, 0.62f, 1.0f, 0, 0.5f, 1.0f};
  pfcParams = {7, 0, {50, 35, 50, 10}, 0, 0.5f, 1.0f};

  for (int i = 0; i < NUM_PATCH_SLOTS; i++) {
    memcpy(&sfPatches[i], &sfParams, sizeof(sfParams));
    memcpy(&wcPatches[i], &wcParams, sizeof(wcParams));
    memcpy(&mrPatches[i], &mrParams, sizeof(mrParams));
    memcpy(&mlPatches[i], &mlParams, sizeof(mlParams));
    memcpy(&wchPatches[i], &wchParams, sizeof(wchParams));
    memcpy(&swPatches[i], &swParams, sizeof(swParams));
    memcpy(&wcoPatches[i], &wcoParams, sizeof(wcoParams));
    memcpy(&paPatches[i], &paParams, sizeof(paParams));
    memcpy(&kdPatches[i], &kdParams, sizeof(kdParams));
    memcpy(&bdPatches[i], &bdParams, sizeof(bdParams));
    memcpy(&fdPatches[i], &fdParams, sizeof(fdParams));
    memcpy(&snPatches[i], &snParams, sizeof(snParams));
    memcpy(&sqPatches[i], &sqParams, sizeof(sqParams));
    memcpy(&pfPatches[i], &pfParams, sizeof(pfParams));
    memcpy(&fsPatches[i], &fsParams, sizeof(fsParams));
    memcpy(&f2Patches[i], &f2Params, sizeof(f2Params));
    memcpy(&pfcPatches[i], &pfcParams, sizeof(pfcParams));
  }
  for (int animation = 0; animation < NUM_ANIMATIONS; animation++) {
    colorParams[animation] = {0, 360, 0};
    if (animation == BAYER_FLOW) colorParams[animation] = {300, 240, 5};
    if (animation == FLOYD_FLOW) colorParams[animation] = {0, 300, 3};
    if (animation == SNAKE) colorParams[animation] = {105, 80, 1};
    if (animation == SQUARES) colorParams[animation] = {250, 360, 0};
    if (animation == PIXEL_AVALANCHE) colorParams[animation] = {0, 360, 6};
    if (animation == PERSPECTIVE_FLOW) colorParams[animation] = {0, 360, 6};
    if (animation == FLOW_SPHERE) colorParams[animation] = {205, 290, 5};
    if (animation == FLOW_FIELD_2D) colorParams[animation] = {175, 300, 5};
    if (animation == PATTERNFLOW_COLLECTION) colorParams[animation] = {195, 360, 5};
    for (int patch = 0; patch < NUM_PATCH_SLOTS; patch++) {
      colorPatches[animation][patch] = colorParams[animation];
    }
  }
  memset(defaultPatches, 0, sizeof(defaultPatches));
}

void saveAllMatrixSettings() {
  preferences.begin("matrix-cfg", false);
  preferences.putUInt("version", 15); // WS2812 profiles + Patternflow collection
  preferences.putInt("global_fps", targetFPS);
  preferences.putBool("auto_cycle", autoCycleEnabled);
  preferences.putULong("cycle_int", autoCycleIntervalSecs);
  preferences.putULong("resync_int", autoResyncInterval);
  preferences.putBool("sched_on", scheduleEnabled);
  preferences.putUShort("sched_start", scheduleStartMinutes);
  preferences.putUShort("sched_end", scheduleEndMinutes);
  preferences.putUShort("sched_fadein", scheduleFadeInSeconds);
  preferences.putUShort("sched_fadeout", scheduleFadeOutSeconds);
  preferences.putBytes("sf_p", sfPatches, sizeof(sfPatches));
  preferences.putBytes("wc_p", wcPatches, sizeof(wcPatches));
  preferences.putBytes("mr_p", mrPatches, sizeof(mrPatches));
  preferences.putBytes("ml_p", mlPatches, sizeof(mlPatches));
  preferences.putBytes("wch_p", wchPatches, sizeof(wchPatches));
  preferences.putBytes("sw_p", swPatches, sizeof(swPatches));
  preferences.putBytes("wco_p", wcoPatches, sizeof(wcoPatches));
  preferences.putBytes("pa_p", paPatches, sizeof(paPatches));
  preferences.putBytes("kd_p", kdPatches, sizeof(kdPatches));
  preferences.putBytes("bd_p", bdPatches, sizeof(bdPatches));
  preferences.putBytes("fd_p", fdPatches, sizeof(fdPatches));
  preferences.putBytes("sn_p", snPatches, sizeof(snPatches));
  preferences.putBytes("sq_p", sqPatches, sizeof(sqPatches));
  preferences.putBytes("pf_p", pfPatches, sizeof(pfPatches));
  preferences.putBytes("fs_p", fsPatches, sizeof(fsPatches));
  preferences.putBytes("f2_p", f2Patches, sizeof(f2Patches));
  preferences.putBytes("pfc_p", pfcPatches, sizeof(pfcPatches));
  preferences.putBytes("color_p", colorPatches, sizeof(colorPatches));
  preferences.putBytes("style_p", stylePatches, sizeof(stylePatches));
  preferences.putBytes("custom_p", customPalettePatches,
                       sizeof(customPalettePatches));
  writePalettePatches(preferences);
  preferences.putBytes("tuning_p", colorTuningPatches,
                       sizeof(colorTuningPatches));

  preferences.putBytes("def_p", defaultPatches, sizeof(defaultPatches));
  preferences.end();
}

void loadSettings() {
  initializeStyleDefaults();
  preferences.begin("matrix-cfg", true);
  const uint32_t storedVersion = preferences.getUInt("version", 0);

  if (storedVersion == 14) {
    // Version 15 appends one collection entry while preserving the sixteen
    // existing native patterns as an unchanged prefix in every shared table.
    targetFPS = preferences.getInt("global_fps", 60);
    autoCycleEnabled = preferences.getBool("auto_cycle", true);
    autoCycleIntervalSecs = preferences.getULong("cycle_int", 120);
    autoResyncInterval = preferences.getULong("resync_int", 0);
    initializeDefaultPatches();
    constexpr size_t OLD_ANIMATION_COUNT = 16;
    const bool complete =
      preferences.getBytes("sf_p", sfPatches, sizeof(sfPatches)) == sizeof(sfPatches) &&
      preferences.getBytes("wc_p", wcPatches, sizeof(wcPatches)) == sizeof(wcPatches) &&
      preferences.getBytes("mr_p", mrPatches, sizeof(mrPatches)) == sizeof(mrPatches) &&
      preferences.getBytes("ml_p", mlPatches, sizeof(mlPatches)) == sizeof(mlPatches) &&
      preferences.getBytes("wch_p", wchPatches, sizeof(wchPatches)) == sizeof(wchPatches) &&
      preferences.getBytes("sw_p", swPatches, sizeof(swPatches)) == sizeof(swPatches) &&
      preferences.getBytes("wco_p", wcoPatches, sizeof(wcoPatches)) == sizeof(wcoPatches) &&
      preferences.getBytes("pa_p", paPatches, sizeof(paPatches)) == sizeof(paPatches) &&
      preferences.getBytes("kd_p", kdPatches, sizeof(kdPatches)) == sizeof(kdPatches) &&
      preferences.getBytes("bd_p", bdPatches, sizeof(bdPatches)) == sizeof(bdPatches) &&
      preferences.getBytes("fd_p", fdPatches, sizeof(fdPatches)) == sizeof(fdPatches) &&
      preferences.getBytes("sn_p", snPatches, sizeof(snPatches)) == sizeof(snPatches) &&
      preferences.getBytes("sq_p", sqPatches, sizeof(sqPatches)) == sizeof(sqPatches) &&
      preferences.getBytes("pf_p", pfPatches, sizeof(pfPatches)) == sizeof(pfPatches) &&
      preferences.getBytes("fs_p", fsPatches, sizeof(fsPatches)) == sizeof(fsPatches) &&
      preferences.getBytes("f2_p", f2Patches, sizeof(f2Patches)) == sizeof(f2Patches) &&
      preferences.getBytes("color_p", colorPatches,
        sizeof(ColorParams) * OLD_ANIMATION_COUNT * NUM_PATCH_SLOTS) ==
        sizeof(ColorParams) * OLD_ANIMATION_COUNT * NUM_PATCH_SLOTS &&
      preferences.getBytes("def_p", defaultPatches,
        sizeof(int) * OLD_ANIMATION_COUNT) == sizeof(int) * OLD_ANIMATION_COUNT;
    preferences.getBytes("style_p", stylePatches,
      sizeof(PatternStyleParams) * OLD_ANIMATION_COUNT * NUM_PATCH_SLOTS);
    preferences.getBytes("custom_p", customPalettePatches,
      sizeof(uint32_t) * OLD_ANIMATION_COUNT * NUM_PATCH_SLOTS * 4);
    readPalettePatches(preferences);
    preferences.getBytes("tuning_p", colorTuningPatches,
      sizeof(ColorTuningParams) * OLD_ANIMATION_COUNT * NUM_PATCH_SLOTS);

    preferences.end();
    if (complete) {
      Serial.println("Migrated version 14 settings and added Patternflow Collection.");
      saveAllMatrixSettings();
    } else {
      Serial.println("Version 14 patch data was incomplete; restoring defaults.");
      initializeDefaultPatches();
      saveAllMatrixSettings();
    }
  } else if (storedVersion == 13) {
    // Append 2D Flow Field while preserving all fifteen version-13 patterns.
    // The new animation is the final array element, so every old table can be
    // loaded safely as a contiguous prefix after the new defaults are built.
    targetFPS = preferences.getInt("global_fps", 60);
    autoCycleEnabled = preferences.getBool("auto_cycle", true);
    autoCycleIntervalSecs = preferences.getULong("cycle_int", 120);
    autoResyncInterval = preferences.getULong("resync_int", 0);
    initializeDefaultPatches();
    constexpr size_t OLD_ANIMATION_COUNT = 15;
    const bool complete =
      preferences.getBytes("sf_p", sfPatches, sizeof(sfPatches)) == sizeof(sfPatches) &&
      preferences.getBytes("wc_p", wcPatches, sizeof(wcPatches)) == sizeof(wcPatches) &&
      preferences.getBytes("mr_p", mrPatches, sizeof(mrPatches)) == sizeof(mrPatches) &&
      preferences.getBytes("ml_p", mlPatches, sizeof(mlPatches)) == sizeof(mlPatches) &&
      preferences.getBytes("wch_p", wchPatches, sizeof(wchPatches)) == sizeof(wchPatches) &&
      preferences.getBytes("sw_p", swPatches, sizeof(swPatches)) == sizeof(swPatches) &&
      preferences.getBytes("wco_p", wcoPatches, sizeof(wcoPatches)) == sizeof(wcoPatches) &&
      preferences.getBytes("pa_p", paPatches, sizeof(paPatches)) == sizeof(paPatches) &&
      preferences.getBytes("kd_p", kdPatches, sizeof(kdPatches)) == sizeof(kdPatches) &&
      preferences.getBytes("bd_p", bdPatches, sizeof(bdPatches)) == sizeof(bdPatches) &&
      preferences.getBytes("fd_p", fdPatches, sizeof(fdPatches)) == sizeof(fdPatches) &&
      preferences.getBytes("sn_p", snPatches, sizeof(snPatches)) == sizeof(snPatches) &&
      preferences.getBytes("sq_p", sqPatches, sizeof(sqPatches)) == sizeof(sqPatches) &&
      preferences.getBytes("pf_p", pfPatches, sizeof(pfPatches)) == sizeof(pfPatches) &&
      preferences.getBytes("fs_p", fsPatches, sizeof(fsPatches)) == sizeof(fsPatches) &&
      preferences.getBytes("color_p", colorPatches,
        sizeof(ColorParams) * OLD_ANIMATION_COUNT * NUM_PATCH_SLOTS) ==
        sizeof(ColorParams) * OLD_ANIMATION_COUNT * NUM_PATCH_SLOTS &&
      preferences.getBytes("def_p", defaultPatches,
        sizeof(int) * OLD_ANIMATION_COUNT) == sizeof(int) * OLD_ANIMATION_COUNT;
    preferences.getBytes("style_p", stylePatches,
      sizeof(PatternStyleParams) * OLD_ANIMATION_COUNT * NUM_PATCH_SLOTS);
    preferences.getBytes("custom_p", customPalettePatches,
      sizeof(uint32_t) * OLD_ANIMATION_COUNT * NUM_PATCH_SLOTS * 4);
    readPalettePatches(preferences);
    preferences.getBytes("tuning_p", colorTuningPatches,
      sizeof(ColorTuningParams) * OLD_ANIMATION_COUNT * NUM_PATCH_SLOTS);

    preferences.end();
    if (complete) {
      Serial.println("Migrated version 13 settings and added 2D Flow Field.");
      saveAllMatrixSettings();
    } else {
      Serial.println("Version 13 patch data was incomplete; restoring defaults.");
      initializeDefaultPatches();
      saveAllMatrixSettings();
    }
  } else if (storedVersion == 11 || storedVersion == 12 || storedVersion == 15) {
    targetFPS = preferences.getInt("global_fps", 60);
    autoCycleEnabled = preferences.getBool("auto_cycle", true);
    autoCycleIntervalSecs = preferences.getULong("cycle_int", 120);
    autoResyncInterval = preferences.getULong("resync_int", 0);

    const bool complete =
      preferences.getBytes("sf_p", sfPatches, sizeof(sfPatches)) == sizeof(sfPatches) &&
      preferences.getBytes("wc_p", wcPatches, sizeof(wcPatches)) == sizeof(wcPatches) &&
      preferences.getBytes("mr_p", mrPatches, sizeof(mrPatches)) == sizeof(mrPatches) &&
      preferences.getBytes("ml_p", mlPatches, sizeof(mlPatches)) == sizeof(mlPatches) &&
      preferences.getBytes("wch_p", wchPatches, sizeof(wchPatches)) == sizeof(wchPatches) &&
      preferences.getBytes("sw_p", swPatches, sizeof(swPatches)) == sizeof(swPatches) &&
      preferences.getBytes("wco_p", wcoPatches, sizeof(wcoPatches)) == sizeof(wcoPatches) &&
      preferences.getBytes("pa_p", paPatches, sizeof(paPatches)) == sizeof(paPatches) &&
      preferences.getBytes("kd_p", kdPatches, sizeof(kdPatches)) == sizeof(kdPatches) &&
      preferences.getBytes("bd_p", bdPatches, sizeof(bdPatches)) == sizeof(bdPatches) &&
      preferences.getBytes("fd_p", fdPatches, sizeof(fdPatches)) == sizeof(fdPatches) &&
      preferences.getBytes("sn_p", snPatches, sizeof(snPatches)) == sizeof(snPatches) &&
      preferences.getBytes("sq_p", sqPatches, sizeof(sqPatches)) == sizeof(sqPatches) &&
      preferences.getBytes("pf_p", pfPatches, sizeof(pfPatches)) == sizeof(pfPatches) &&
      preferences.getBytes("fs_p", fsPatches, sizeof(fsPatches)) == sizeof(fsPatches) &&
      preferences.getBytes("f2_p", f2Patches, sizeof(f2Patches)) == sizeof(f2Patches) &&
      preferences.getBytes("pfc_p", pfcPatches, sizeof(pfcPatches)) == sizeof(pfcPatches) &&
      preferences.getBytes("color_p", colorPatches, sizeof(colorPatches)) == sizeof(colorPatches) &&
      preferences.getBytes("def_p", defaultPatches, sizeof(defaultPatches)) == sizeof(defaultPatches);
    const bool extrasComplete =
      preferences.getBytes("style_p", stylePatches, sizeof(stylePatches)) == sizeof(stylePatches) &&
      preferences.getBytes("custom_p", customPalettePatches, sizeof(customPalettePatches)) == sizeof(customPalettePatches);
    const bool paletteStopsComplete =
      readPalettePatches(preferences) == sizeof(paletteStopPatches);
    const bool tuningComplete =
      preferences.getBytes("tuning_p", colorTuningPatches,
                           sizeof(colorTuningPatches)) == sizeof(colorTuningPatches);
    preferences.end();

    if (!complete) {
      Serial.println("Stored patch data was incomplete; restoring defaults.");
      initializeDefaultPatches();
      saveAllMatrixSettings();
    } else {
      Serial.println("Loaded matrix settings and patches from flash.");
      if (!paletteStopsComplete && extrasComplete) {
        // Preserve the former editable Custom palette as palette slot 6 while
        // initializing editable stops for all six former built-in palettes.
        for (int animation = 0; animation < NUM_ANIMATIONS; animation++) {
          for (int patch = 0; patch < NUM_PATCH_SLOTS; patch++) {
            memcpy(paletteStopPatches[animation][patch][6],
                   customPalettePatches[animation][patch],
                   sizeof(customPalettePatches[animation][patch]));
          }
        }
      }
      if (!extrasComplete || !paletteStopsComplete ||
          !tuningComplete) {
        // Version 10 originally predated editable palettes and spatial style.
        // These keys are optional so existing patches remain compatible.
        for (int patch = 0; patch < NUM_PATCH_SLOTS; patch++) {
          colorPatches[PIXEL_AVALANCHE][patch].palette = 6;
          colorPatches[PERSPECTIVE_FLOW][patch].palette = 6;
        }
        Serial.println("Added optional style, palette, or color-tuning defaults.");
        saveAllMatrixSettings();
      }
    }
  } else if (storedVersion == 10) {
    // Append Flow Sphere without discarding any of the fourteen existing
    // pattern patches. Because the new animation is appended, each old table
    // is a contiguous prefix of the new table and can be loaded in place.
    targetFPS = preferences.getInt("global_fps", 60);
    autoCycleEnabled = preferences.getBool("auto_cycle", true);
    autoCycleIntervalSecs = preferences.getULong("cycle_int", 120);
    autoResyncInterval = preferences.getULong("resync_int", 0);
    initializeDefaultPatches();
    constexpr size_t OLD_ANIMATION_COUNT = 14;
    const bool complete =
      preferences.getBytes("sf_p", sfPatches, sizeof(sfPatches)) == sizeof(sfPatches) &&
      preferences.getBytes("wc_p", wcPatches, sizeof(wcPatches)) == sizeof(wcPatches) &&
      preferences.getBytes("mr_p", mrPatches, sizeof(mrPatches)) == sizeof(mrPatches) &&
      preferences.getBytes("ml_p", mlPatches, sizeof(mlPatches)) == sizeof(mlPatches) &&
      preferences.getBytes("wch_p", wchPatches, sizeof(wchPatches)) == sizeof(wchPatches) &&
      preferences.getBytes("sw_p", swPatches, sizeof(swPatches)) == sizeof(swPatches) &&
      preferences.getBytes("wco_p", wcoPatches, sizeof(wcoPatches)) == sizeof(wcoPatches) &&
      preferences.getBytes("pa_p", paPatches, sizeof(paPatches)) == sizeof(paPatches) &&
      preferences.getBytes("kd_p", kdPatches, sizeof(kdPatches)) == sizeof(kdPatches) &&
      preferences.getBytes("bd_p", bdPatches, sizeof(bdPatches)) == sizeof(bdPatches) &&
      preferences.getBytes("fd_p", fdPatches, sizeof(fdPatches)) == sizeof(fdPatches) &&
      preferences.getBytes("sn_p", snPatches, sizeof(snPatches)) == sizeof(snPatches) &&
      preferences.getBytes("sq_p", sqPatches, sizeof(sqPatches)) == sizeof(sqPatches) &&
      preferences.getBytes("pf_p", pfPatches, sizeof(pfPatches)) == sizeof(pfPatches) &&
      preferences.getBytes("color_p", colorPatches,
        sizeof(ColorParams) * OLD_ANIMATION_COUNT * NUM_PATCH_SLOTS) ==
        sizeof(ColorParams) * OLD_ANIMATION_COUNT * NUM_PATCH_SLOTS &&
      preferences.getBytes("def_p", defaultPatches,
        sizeof(int) * OLD_ANIMATION_COUNT) == sizeof(int) * OLD_ANIMATION_COUNT;
    preferences.getBytes("style_p", stylePatches,
      sizeof(PatternStyleParams) * OLD_ANIMATION_COUNT * NUM_PATCH_SLOTS);
    preferences.getBytes("custom_p", customPalettePatches,
      sizeof(uint32_t) * OLD_ANIMATION_COUNT * NUM_PATCH_SLOTS * 4);
    readPalettePatches(preferences);
    preferences.getBytes("tuning_p", colorTuningPatches,
      sizeof(ColorTuningParams) * OLD_ANIMATION_COUNT * NUM_PATCH_SLOTS);
    preferences.end();

    if (complete) {
      Serial.println("Migrated version 10 settings and added Flow Sphere.");
      saveAllMatrixSettings();
    } else {
      Serial.println("Version 10 patch data was incomplete; restoring defaults.");
      initializeDefaultPatches();
      saveAllMatrixSettings();
    }
  } else if (storedVersion == 9) {
    // Append Perspective Flow while retaining all thirteen current patterns.
    targetFPS = preferences.getInt("global_fps", 60);
    autoCycleEnabled = preferences.getBool("auto_cycle", true);
    autoCycleIntervalSecs = preferences.getULong("cycle_int", 120);
    autoResyncInterval = preferences.getULong("resync_int", 0);
    ColorParams oldColorPatches[13][NUM_PATCH_SLOTS];
    int oldDefaults[13] = {0};
    const bool complete =
      preferences.getBytes("sf_p", sfPatches, sizeof(sfPatches)) == sizeof(sfPatches) &&
      preferences.getBytes("wc_p", wcPatches, sizeof(wcPatches)) == sizeof(wcPatches) &&
      preferences.getBytes("mr_p", mrPatches, sizeof(mrPatches)) == sizeof(mrPatches) &&
      preferences.getBytes("ml_p", mlPatches, sizeof(mlPatches)) == sizeof(mlPatches) &&
      preferences.getBytes("wch_p", wchPatches, sizeof(wchPatches)) == sizeof(wchPatches) &&
      preferences.getBytes("sw_p", swPatches, sizeof(swPatches)) == sizeof(swPatches) &&
      preferences.getBytes("wco_p", wcoPatches, sizeof(wcoPatches)) == sizeof(wcoPatches) &&
      preferences.getBytes("pa_p", paPatches, sizeof(paPatches)) == sizeof(paPatches) &&
      preferences.getBytes("kd_p", kdPatches, sizeof(kdPatches)) == sizeof(kdPatches) &&
      preferences.getBytes("bd_p", bdPatches, sizeof(bdPatches)) == sizeof(bdPatches) &&
      preferences.getBytes("fd_p", fdPatches, sizeof(fdPatches)) == sizeof(fdPatches) &&
      preferences.getBytes("sn_p", snPatches, sizeof(snPatches)) == sizeof(snPatches) &&
      preferences.getBytes("sq_p", sqPatches, sizeof(sqPatches)) == sizeof(sqPatches) &&
      preferences.getBytes("color_p", oldColorPatches, sizeof(oldColorPatches)) == sizeof(oldColorPatches) &&
      preferences.getBytes("def_p", oldDefaults, sizeof(oldDefaults)) == sizeof(oldDefaults);
    preferences.end();

    if (complete) {
      pfParams = {7, 6, 3.0f, 28.0f, 1.0f, 0, 0.5f, 1.0f};
      for (int patch = 0; patch < NUM_PATCH_SLOTS; patch++) {
        snPatches[patch].cell = 1;
        pfPatches[patch] = pfParams;
      }
      memcpy(colorPatches, oldColorPatches, sizeof(oldColorPatches));
      colorParams[PERSPECTIVE_FLOW] = {0, 360, 6};
      for (int patch = 0; patch < NUM_PATCH_SLOTS; patch++) {
        colorPatches[PIXEL_AVALANCHE][patch].palette = 6;
        colorPatches[PERSPECTIVE_FLOW][patch] = colorParams[PERSPECTIVE_FLOW];
      }
      memset(defaultPatches, 0, sizeof(defaultPatches));
      memcpy(defaultPatches, oldDefaults, sizeof(oldDefaults));
      Serial.println("Migrated version 9 settings with Perspective Flow and one-pixel Snake.");
      saveAllMatrixSettings();
    } else {
      Serial.println("Version 9 patch data was incomplete; restoring defaults.");
      initializeDefaultPatches();
      saveAllMatrixSettings();
    }
  } else if (storedVersion == 8) {
    // Preserve every existing palette/dither patch while appending the two
    // new native patterns and their defaults.
    targetFPS = preferences.getInt("global_fps", 60);
    autoCycleEnabled = preferences.getBool("auto_cycle", true);
    autoCycleIntervalSecs = preferences.getULong("cycle_int", 120);
    autoResyncInterval = preferences.getULong("resync_int", 0);
    ColorParams oldColorPatches[11][NUM_PATCH_SLOTS];
    int oldDefaults[11] = {0};
    const bool complete =
      preferences.getBytes("sf_p", sfPatches, sizeof(sfPatches)) == sizeof(sfPatches) &&
      preferences.getBytes("wc_p", wcPatches, sizeof(wcPatches)) == sizeof(wcPatches) &&
      preferences.getBytes("mr_p", mrPatches, sizeof(mrPatches)) == sizeof(mrPatches) &&
      preferences.getBytes("ml_p", mlPatches, sizeof(mlPatches)) == sizeof(mlPatches) &&
      preferences.getBytes("wch_p", wchPatches, sizeof(wchPatches)) == sizeof(wchPatches) &&
      preferences.getBytes("sw_p", swPatches, sizeof(swPatches)) == sizeof(swPatches) &&
      preferences.getBytes("wco_p", wcoPatches, sizeof(wcoPatches)) == sizeof(wcoPatches) &&
      preferences.getBytes("pa_p", paPatches, sizeof(paPatches)) == sizeof(paPatches) &&
      preferences.getBytes("kd_p", kdPatches, sizeof(kdPatches)) == sizeof(kdPatches) &&
      preferences.getBytes("bd_p", bdPatches, sizeof(bdPatches)) == sizeof(bdPatches) &&
      preferences.getBytes("fd_p", fdPatches, sizeof(fdPatches)) == sizeof(fdPatches) &&
      preferences.getBytes("color_p", oldColorPatches, sizeof(oldColorPatches)) == sizeof(oldColorPatches) &&
      preferences.getBytes("def_p", oldDefaults, sizeof(oldDefaults)) == sizeof(oldDefaults);
    preferences.end();

    if (complete) {
      snParams = {7, 1, 96, 8.0f, 0.82f, 0, 0.5f, 1.0f};
      sqParams = {7, 10, 1, 1.0f, 1.0f, 0.16f, 0, 0.5f, 1.0f};
      pfParams = {7, 6, 3.0f, 28.0f, 1.0f, 0, 0.5f, 1.0f};
      for (int patch = 0; patch < NUM_PATCH_SLOTS; patch++) {
        snPatches[patch] = snParams;
        sqPatches[patch] = sqParams;
        pfPatches[patch] = pfParams;
      }
      memcpy(colorPatches, oldColorPatches, sizeof(oldColorPatches));
      colorParams[SNAKE] = {105, 80, 1};
      colorParams[SQUARES] = {250, 360, 0};
      colorParams[PERSPECTIVE_FLOW] = {0, 360, 6};
      for (int patch = 0; patch < NUM_PATCH_SLOTS; patch++) {
        colorPatches[PIXEL_AVALANCHE][patch].palette = 6;
        colorPatches[SNAKE][patch] = colorParams[SNAKE];
        colorPatches[SQUARES][patch] = colorParams[SQUARES];
        colorPatches[PERSPECTIVE_FLOW][patch] = colorParams[PERSPECTIVE_FLOW];
      }
      memset(defaultPatches, 0, sizeof(defaultPatches));
      memcpy(defaultPatches, oldDefaults, sizeof(oldDefaults));
      Serial.println("Migrated version 8 settings with Snake and Squares.");
      saveAllMatrixSettings();
    } else {
      Serial.println("Version 8 patch data was incomplete; restoring defaults.");
      initializeDefaultPatches();
      saveAllMatrixSettings();
    }
  } else if (storedVersion == 7) {
    // Preserve all existing nine-pattern patches while adding default palette
    // state and the two new dither patterns.
    targetFPS = preferences.getInt("global_fps", 60);
    autoCycleEnabled = preferences.getBool("auto_cycle", true);
    autoCycleIntervalSecs = preferences.getULong("cycle_int", 120);
    autoResyncInterval = preferences.getULong("resync_int", 0);
    int oldDefaults[9] = {0};
    const bool complete =
      preferences.getBytes("sf_p", sfPatches, sizeof(sfPatches)) == sizeof(sfPatches) &&
      preferences.getBytes("wc_p", wcPatches, sizeof(wcPatches)) == sizeof(wcPatches) &&
      preferences.getBytes("mr_p", mrPatches, sizeof(mrPatches)) == sizeof(mrPatches) &&
      preferences.getBytes("ml_p", mlPatches, sizeof(mlPatches)) == sizeof(mlPatches) &&
      preferences.getBytes("wch_p", wchPatches, sizeof(wchPatches)) == sizeof(wchPatches) &&
      preferences.getBytes("sw_p", swPatches, sizeof(swPatches)) == sizeof(swPatches) &&
      preferences.getBytes("wco_p", wcoPatches, sizeof(wcoPatches)) == sizeof(wcoPatches) &&
      preferences.getBytes("pa_p", paPatches, sizeof(paPatches)) == sizeof(paPatches) &&
      preferences.getBytes("kd_p", kdPatches, sizeof(kdPatches)) == sizeof(kdPatches) &&
      preferences.getBytes("def_p", oldDefaults, sizeof(oldDefaults)) == sizeof(oldDefaults);
    preferences.end();

    if (complete) {
      bdParams = {7, 7, 4, 1.0f, 0.8f, 0, 0.5f, 1.0f};
      fdParams = {7, 4, 7, 1.0f, 1.0f, 0, 0.5f, 1.0f};
      snParams = {7, 1, 96, 8.0f, 0.82f, 0, 0.5f, 1.0f};
      sqParams = {7, 10, 1, 1.0f, 1.0f, 0.16f, 0, 0.5f, 1.0f};
      pfParams = {7, 6, 3.0f, 28.0f, 1.0f, 0, 0.5f, 1.0f};
      for (int patch = 0; patch < NUM_PATCH_SLOTS; patch++) {
        bdPatches[patch] = bdParams;
        fdPatches[patch] = fdParams;
        snPatches[patch] = snParams;
        sqPatches[patch] = sqParams;
        pfPatches[patch] = pfParams;
      }
      for (int animation = 0; animation < NUM_ANIMATIONS; animation++) {
        colorParams[animation] = {0, 360, 0};
        if (animation == BAYER_FLOW) colorParams[animation] = {300, 240, 5};
        if (animation == FLOYD_FLOW) colorParams[animation] = {0, 300, 3};
        if (animation == SNAKE) colorParams[animation] = {105, 80, 1};
        if (animation == SQUARES) colorParams[animation] = {250, 360, 0};
        if (animation == PIXEL_AVALANCHE) colorParams[animation] = {0, 360, 6};
        if (animation == PERSPECTIVE_FLOW) colorParams[animation] = {0, 360, 6};
        for (int patch = 0; patch < NUM_PATCH_SLOTS; patch++) {
          colorPatches[animation][patch] = colorParams[animation];
        }
      }
      memset(defaultPatches, 0, sizeof(defaultPatches));
      memcpy(defaultPatches, oldDefaults, sizeof(oldDefaults));
      Serial.println("Migrated version 7 patches to palette/dither settings.");
      saveAllMatrixSettings();
    } else {
      Serial.println("Version 7 patch data was incomplete; restoring defaults.");
      initializeDefaultPatches();
      saveAllMatrixSettings();
    }
  } else {
    preferences.end();
    Serial.println("No compatible settings found; creating defaults.");
    initializeDefaultPatches();
    saveAllMatrixSettings();
  }

  preferences.begin("matrix-cfg", true);
  scheduleEnabled = preferences.getBool("sched_on", false);
  scheduleStartMinutes = preferences.getUShort("sched_start", 18 * 60);
  scheduleEndMinutes = preferences.getUShort("sched_end", 2 * 60);
  scheduleFadeInSeconds = preferences.getUShort("sched_fadein", 30);
  scheduleFadeOutSeconds = preferences.getUShort("sched_fadeout", 5);
  preferences.end();

  targetFPS = constrain(targetFPS, 1, 240);
  autoCycleIntervalSecs = constrain(autoCycleIntervalSecs, 1UL, 86400UL);
  autoResyncInterval = constrain(autoResyncInterval, 0UL, 86400UL);
  scheduleStartMinutes = constrain(scheduleStartMinutes, 0, 1439);
  scheduleEndMinutes = constrain(scheduleEndMinutes, 0, 1439);
  scheduleFadeInSeconds = constrain(scheduleFadeInSeconds, 1, 3600);
  scheduleFadeOutSeconds = constrain(scheduleFadeOutSeconds, 1, 3600);

  // Migration from the MAX7219 version: periodic re-sync is unnecessary on
  // the current self-refreshing/latched display backends.
  if (autoResyncInterval != 0) {
    autoResyncInterval = 0;
    preferences.begin("matrix-cfg", false);
    preferences.putULong("resync_int", 0);
    preferences.end();
    Serial.println("Disabled legacy automatic display re-sync.");
  }

  for (int i = 0; i < NUM_ANIMATIONS; i++) {
    defaultPatches[i] = constrain(defaultPatches[i], 0, NUM_PATCH_SLOTS - 1);
  }

  loadPatch(STARFIELD, defaultPatches[STARFIELD]);
}

void applyCurrentBrightness() {
  // Current backends have no MAX7219-style intensity register. Pattern
  // brightness is applied in makeColor()/makeWhite() per rendered pixel.
}

void loadPatch(Animation anim, int patchIndex) {
  if (patchIndex < 0 || patchIndex >= NUM_PATCH_SLOTS || !isAvailableEngine(anim)) return;
  if(anim==PATTERNFLOW_COLLECTION){loadVariantPatch(pfcParams.variant,patchIndex);return;}
  patternflowPack.reset(255);

  switch (anim) {
    case STARFIELD:           memcpy(&sfParams, &sfPatches[patchIndex], sizeof(sfParams)); break;
    case WARPED_CHECKERBOARD: memcpy(&wcParams, &wcPatches[patchIndex], sizeof(wcParams)); break;
    case MOIRE_RINGS:         memcpy(&mrParams, &mrPatches[patchIndex], sizeof(mrParams)); break;
    case MOIRE_LINES:         memcpy(&mlParams, &mlPatches[patchIndex], sizeof(mlParams)); break;
    case WARPING_CHECKERS:    memcpy(&wchParams, &wchPatches[patchIndex], sizeof(wchParams)); break;
    case SHOCKWAVE:           memcpy(&swParams, &swPatches[patchIndex], sizeof(swParams)); break;
    case WARP_CORRIDORS:      memcpy(&wcoParams, &wcoPatches[patchIndex], sizeof(wcoParams)); break;
    case PIXEL_AVALANCHE:     memcpy(&paParams, &paPatches[patchIndex], sizeof(paParams)); break;
    case KALEIDOSCOPE:        memcpy(&kdParams, &kdPatches[patchIndex], sizeof(kdParams)); break;
    case BAYER_FLOW:          memcpy(&bdParams, &bdPatches[patchIndex], sizeof(bdParams)); break;
    case FLOYD_FLOW:          memcpy(&fdParams, &fdPatches[patchIndex], sizeof(fdParams)); break;
    case SNAKE:               memcpy(&snParams, &snPatches[patchIndex], sizeof(snParams)); break;
    case SQUARES:             memcpy(&sqParams, &sqPatches[patchIndex], sizeof(sqParams)); break;
    case PERSPECTIVE_FLOW:    memcpy(&pfParams, &pfPatches[patchIndex], sizeof(pfParams)); break;
    case FLOW_SPHERE:         memcpy(&fsParams, &fsPatches[patchIndex], sizeof(fsParams)); break;
    case FLOW_FIELD_2D:       memcpy(&f2Params, &f2Patches[patchIndex], sizeof(f2Params)); break;
    case PATTERNFLOW_COLLECTION: memcpy(&pfcParams, &pfcPatches[patchIndex], sizeof(pfcParams)); break;
  }
  if (anim == SNAKE) snParams.cell = 1;
  colorParams[(int)anim] = colorPatches[(int)anim][patchIndex];
  colorTuning[(int)anim] = colorTuningPatches[(int)anim][patchIndex];
  styleParams[(int)anim] = stylePatches[(int)anim][patchIndex];
  memcpy(customPalette[(int)anim],
         customPalettePatches[(int)anim][patchIndex],
         sizeof(customPalette[(int)anim]));
  memcpy(paletteStops[(int)anim],
         paletteStopPatches[(int)anim][patchIndex],
         sizeof(paletteStops[(int)anim]));
  applySoukanDefault((int)anim,0,patchIndex);
  huePaletteAnimation = -1;

  if (anim == PIXEL_AVALANCHE) {
    memset(avalancheGrid, 0, sizeof(avalancheGrid));
    avalancheTimeSinceUpdate = 0;
    avalancheWhitePhase = true;
  }
  if (anim == FLOYD_FLOW) {
    memset(diffusionErrorA, 0, sizeof(diffusionErrorA));
    memset(diffusionErrorB, 0, sizeof(diffusionErrorB));
  }
  if (anim == SNAKE) resetSnake(snParams.cell);
  if (anim == FLOW_SPHERE) flowSphereTime = 0.0f;
  if (anim == FLOW_FIELD_2D) {
    flowField2DTime = 0.0f;
    flowField2DInitialized = false;
  }
  if (anim == PATTERNFLOW_COLLECTION) {
    pfcParams.variant = constrain(pfcParams.variant, 0,
                                  PATTERNFLOW_PATTERN_COUNT - 1);
    patternflowPack.reset((uint8_t)pfcParams.variant);
  }

  applyCurrentBrightness();
  const unsigned long frameDuration =
    (targetFPS > 0) ? (1000000UL / (unsigned long)targetFPS) : 1000000UL;
  const unsigned long now = micros();
  lastFrameTime = now - frameDuration;
  nextFrameDueTime = now;
}

void setMatrixPoint(int y, int x, bool state) {
  if (!state) return;
  setMatrixPointColor(y, x, makeWhite(0.75f, 5));
}

uint8_t quantizeChannelForTest(uint8_t value, uint8_t bits) {
  const uint16_t maximum = (1U << bits) - 1U;
  const uint16_t level = ((uint16_t)value * maximum + 127U) / 255U;
  return (uint8_t)((level * 255U + maximum / 2U) / maximum);
}

void renderDepthComparison(float timeSeconds) {
  matrix.fillScreen(0);
  matrix.setTextWrap(false);
  matrix.setTextSize(1);
  matrix.setTextColor(matrix.color565(64, 64, 64));
  matrix.setCursor(5, 0);
  matrix.print("3 BIT");
  matrix.setCursor(37, 0);
  matrix.print("4 BIT");

  for (int x = 0; x < SCREEN_WIDTH; x++) {
    if (x == 31 || x == 32) {
      matrix.drawFastVLine(x, 8, SCREEN_HEIGHT - 8,
                           matrix.color565(70, 70, 70));
      continue;
    }
    const bool threeBit = x < 31;
    const int localX = threeBit ? x : x - 33;
    const uint8_t bits = threeBit ? 3 : 4;

    const uint8_t fullGray = quantizeChannelForTest(
      (uint8_t)(localX * 128 / 30), bits);
    // Low-light ramp makes shadow-band loss especially easy to compare.
    const uint8_t darkGray = quantizeChannelForTest(
      (uint8_t)(localX * 64 / 30), bits);
    const float phase = localX * (2.0f * PI / 31.0f) + timeSeconds * 1.3f;
    const uint8_t red = quantizeChannelForTest(
      (uint8_t)(64.f + 64.f * fastSin(phase)), bits);
    const uint8_t green = quantizeChannelForTest(
      (uint8_t)(64.f + 64.f * fastSin(phase + 2.0944f)), bits);
    const uint8_t blue = quantizeChannelForTest(
      (uint8_t)(64.f + 64.f * fastSin(phase + 4.1888f)), bits);
    matrix.drawFastVLine(x, 8, 17, matrix.color565(fullGray, fullGray, fullGray));
    matrix.drawFastVLine(x, 25, 16, matrix.color565(darkGray, darkGray, darkGray));
    matrix.drawFastVLine(x, 41, 23, matrix.color565(red, green, blue));
  }
}

uint16_t lowDepthDitherColor(uint16_t color, int x, int y) {
#if MATRIX_COLOR_DEPTH <= 3 && MATRIX_LOW_LIGHT_DITHER
  if (color == 0) return 0;
  static const uint8_t bayer4x4[16] = {
     0,  8,  2, 10,
    12,  4, 14,  6,
     3, 11,  1,  9,
    15,  7, 13,  5
  };
  const uint8_t threshold = bayer4x4[((y & 3) << 2) | (x & 3)];
  auto quantize = [threshold](uint8_t value) -> uint8_t {
    const uint16_t scaled = (uint16_t)value * 7U;
    uint8_t level = scaled / 255U;
    const uint16_t remainder = scaled % 255U;
    if (level < 7 && remainder * 16U > (uint16_t)threshold * 255U + 127U) {
      level++;
    }
    return level;
  };
  const uint8_t red8 = (uint8_t)((((color >> 11) & 0x1F) * 255U + 15U) / 31U);
  const uint8_t green8 = (uint8_t)((((color >> 5) & 0x3F) * 255U + 31U) / 63U);
  const uint8_t blue8 = (uint8_t)(((color & 0x1F) * 255U + 15U) / 31U);
  const uint8_t red5 = (quantize(red8) * 31U + 3U) / 7U;
  const uint8_t green6 = (quantize(green8) * 63U + 3U) / 7U;
  const uint8_t blue5 = (quantize(blue8) * 31U + 3U) / 7U;
  return ((uint16_t)red5 << 11) | ((uint16_t)green6 << 5) | blue5;
#else
  (void)x;
  (void)y;
  return color;
#endif
}

void setMatrixPointColor(int y, int x, uint16_t color) {
  if (combinedOutputScale < 0.999f) {
    color = scaleRgb565(color, (uint8_t)roundf(combinedOutputScale * 255.0f));
  }
  const PatternStyleParams& style = activeStyleParams;
  if (style.zoom == 1.0f && style.rotation == 0.0f &&
      style.offsetX == 0 && style.offsetY == 0 && style.symmetry == 0) {
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
      const uint16_t output = lowDepthDitherColor(color, x, y);
      matrix.drawPixel(x, y, output);
      previewCapture[y * SCREEN_WIDTH + x] = output;
    }
    return;
  }
  const float centerX = (SCREEN_WIDTH - 1) * 0.5f;
  const float centerY = (SCREEN_HEIGHT - 1) * 0.5f;
  const float sourceX = (x - centerX) * style.zoom;
  const float sourceY = (y - centerY) * style.zoom;
  const int transformedX = (int)roundf(centerX + sourceX * activeStyleCosine -
                                       sourceY * activeStyleSine) + style.offsetX;
  const int transformedY = (int)roundf(centerY + sourceX * activeStyleSine +
                                       sourceY * activeStyleCosine) + style.offsetY;

  auto plot = [&](int px, int py) {
    if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
      const uint16_t output = lowDepthDitherColor(color, px, py);
      matrix.drawPixel(px, py, output);
      previewCapture[py * SCREEN_WIDTH + px] = output;
    }
  };
  plot(transformedX, transformedY);
  if (style.symmetry == 1 || style.symmetry == 3) {
    plot(SCREEN_WIDTH - 1 - transformedX, transformedY);
  }
  if (style.symmetry == 2 || style.symmetry == 3) {
    plot(transformedX, SCREEN_HEIGHT - 1 - transformedY);
  }
  if (style.symmetry == 3) {
    plot(SCREEN_WIDTH - 1 - transformedX,
         SCREEN_HEIGHT - 1 - transformedY);
  }
}

void printPreviewFrame() {
  constexpr int previewWidth = 28;
  constexpr int previewHeight = 28;
  static const char hex[] = "0123456789ABCDEF";
  char row[previewWidth * 2 + 1];
  Serial.printf("USBPREVIEW,%d,%d,", previewWidth, previewHeight);
  for (int py = 0; py < previewHeight; py++) {
    for (int px = 0; px < previewWidth; px++) {
      const int sourceX = min(SCREEN_WIDTH - 1,
        (px * SCREEN_WIDTH + previewWidth / 2) / previewWidth);
      const int sourceY = min(SCREEN_HEIGHT - 1,
        (py * SCREEN_HEIGHT + previewHeight / 2) / previewHeight);
      const uint16_t color = previewCapture[sourceY * SCREEN_WIDTH + sourceX];
      const uint8_t r = (((color >> 11) & 0x1F) * 7 + 15) / 31;
      const uint8_t g = (((color >> 5) & 0x3F) * 7 + 31) / 63;
      const uint8_t b = ((color & 0x1F) * 3 + 15) / 31;
      const uint8_t rgb332 = (r << 5) | (g << 2) | b;
      row[px * 2] = hex[rgb332 >> 4];
      row[px * 2 + 1] = hex[rgb332 & 0x0F];
    }
    row[previewWidth * 2] = '\0';
    Serial.print(row);
  }
  Serial.println();
}

void handlePreviewGet() {
  // Snapshot only on explicit developer requests; no streaming/rendering task.
  server.client().setNoDelay(true);
  static const char hex[]="0123456789abcdef";
  if(server.arg("format")=="rgb565-native"){
    // Exact existing frame buffer, little-endian RGB565; no large JSON copy.
    server.sendHeader("Cache-Control","no-store");server.setContentLength(sizeof(previewCapture));
    server.send(200,"application/octet-stream","");
    server.sendContent(reinterpret_cast<const char*>(previewCapture),sizeof(previewCapture));return;
  }
  if(server.arg("format")=="rgb888-native"){
    // Full-resolution QA snapshot, streamed by row to avoid a 25 KB allocation.
    String prefix=String("{\"width\":")+SCREEN_WIDTH+",\"height\":"+SCREEN_HEIGHT+",\"encoding\":\"rgb888-native\",\"pixels\":\"";
    server.sendHeader("Cache-Control","no-store");server.setContentLength(prefix.length()+SCREEN_WIDTH*SCREEN_HEIGHT*6+2);
    server.send(200,"application/json",prefix);String row;row.reserve(SCREEN_WIDTH*6*16);
    for(int y=0;y<SCREEN_HEIGHT;y++){for(int x=0;x<SCREEN_WIDTH;x++){
      uint16_t c=previewCapture[y*SCREEN_WIDTH+x];uint8_t rgb[3]={uint8_t(((c>>11)&31)*255/31),uint8_t(((c>>5)&63)*255/63),uint8_t((c&31)*255/31)};
      for(auto v:rgb){row+=hex[v>>4];row+=hex[v&15];}
    }if((y&15)==15||y==SCREEN_HEIGHT-1){server.sendContent(row);row="";}}
    server.sendContent("\"}");return;
  }
  const bool binaryRgb=server.arg("format")=="rgb888-binary";
  const bool areaRgb=binaryRgb||server.arg("format")=="rgb888";
  String response;
  response.reserve(binaryRgb?2352:areaRgb?4800:1650);
  response=binaryRgb?"":areaRgb?"{\"width\":28,\"height\":28,\"encoding\":\"rgb888-area\",\"pixels\":\"":
    "{\"width\":28,\"height\":28,\"pixels\":\"";
  for(int y=0;y<28;y++)for(int x=0;x<28;x++){
    if(areaRgb){
      // Exact footprint integration in integer 1/28-pixel units. Unlike
      // nearest-neighbor sampling, every thin line contributes to the tile.
      // This cost is paid only when recording a preview, never per LED frame.
      const int left=x*SCREEN_WIDTH,right=(x+1)*SCREEN_WIDTH;
      const int top=y*SCREEN_HEIGHT,bottom=(y+1)*SCREEN_HEIGHT;
      uint32_t sum[3]={0,0,0};
      for(int sy=top/28;sy<=(bottom-1)/28;sy++)for(int sx=left/28;sx<=(right-1)/28;sx++){
        const int weight=(min(right,(sx+1)*28)-max(left,sx*28))*(min(bottom,(sy+1)*28)-max(top,sy*28));
        const uint16_t c=previewCapture[sy*SCREEN_WIDTH+sx];
        sum[0]+=((c>>11)&31)*weight;sum[1]+=((c>>5)&63)*weight;sum[2]+=(c&31)*weight;
      }
      for(int channel=0;channel<3;channel++){
        const uint32_t divisor=SCREEN_WIDTH*SCREEN_HEIGHT*(channel==1?63:31);
        const uint8_t component=(sum[channel]*255+divisor/2)/divisor;
        if(binaryRgb)response+=char(component);
        else {response+=hex[component>>4];response+=hex[component&15];}
      }
      continue;
    }
    const uint16_t c=previewCapture[(y*SCREEN_HEIGHT/28)*SCREEN_WIDTH+x*SCREEN_WIDTH/28];
    const uint8_t r=(((c>>11)&31)*7+15)/31,g=(((c>>5)&63)*7+31)/63,b=((c&31)*3+15)/31;
    const uint8_t rgb=(r<<5)|(g<<2)|b;
    response+=hex[rgb>>4];response+=hex[rgb&15];
  }
  if(!binaryRgb)response+="\"}";
  server.sendHeader("Cache-Control","no-store");
  server.send(200,binaryRgb?"application/octet-stream":"application/json",response);
}

inline float fastSin(float radians) {
  const int32_t index = (int32_t)(radians * RADIANS_TO_SIN_INDEX);
  return sinLut[(uint32_t)index & SIN_LUT_MASK];
}

inline float fastDistance(float dx, float dy) {
  // Alpha-max-plus-beta-min approximation. Maximum error is small enough for
  // these organic/radial patterns and avoids a sqrt/hypot for every pixel.
  dx = fabsf(dx);
  dy = fabsf(dy);
  const float maximum = max(dx, dy);
  const float minimum = min(dx, dy);
  return maximum + minimum * 0.375f;
}

uint16_t makePaletteColor(float position, uint8_t saturation, uint8_t value,
                          int brightness, const ColorParams& controls) {
  if(Soukan::active(controls.palette))position=position*Soukan::colorScale(currentAnimation,pfcParams.variant)+Soukan::colorOffset(currentAnimation,pfcParams.variant);
  float palettePosition = controls.hue / 360.0f +
                          position * (controls.spread / 360.0f);
  palettePosition -= floorf(palettePosition);
  const float scaled = palettePosition * 4.0f;
  const int first = ((int)scaled) & 3;
  const int second = (first + 1) & 3;
  const float mix = scaled - floorf(scaled);
  const uint8_t paletteIndex = constrain((int)controls.palette, 0, PALETTE_COUNT-1);
  const auto* stops=paletteStops[(int)currentAnimation][paletteIndex];
  const uint32_t a = Soukan::active(paletteIndex)?Soukan::sample(stops,palettePosition):stops[first];
  const uint32_t b = Soukan::active(paletteIndex)?a:stops[second];

  const float red = ((((a >> 16) & 0xFF) * (1.0f - mix)) +
                     (((b >> 16) & 0xFF) * mix)) / 255.0f;
  const float green = ((((a >> 8) & 0xFF) * (1.0f - mix)) +
                       (((b >> 8) & 0xFF) * mix)) / 255.0f;
  const float blue = (((a & 0xFF) * (1.0f - mix)) +
                      ((b & 0xFF) * mix)) / 255.0f;
  const float maximum = max(red, max(green, blue));
  const float minimum = min(red, min(green, blue));
  const float delta = maximum - minimum;
  float hue = 0.0f;
  if (delta > 0.0001f) {
    if (maximum == red) hue = (green - blue) / delta;
    else if (maximum == green) hue = 2.0f + (blue - red) / delta;
    else hue = 4.0f + (red - green) / delta;
    hue /= 6.0f;
    if (hue < 0.0f) hue += 1.0f;
  }
  const ColorTuningParams& tuning = colorTuning[(int)currentAnimation];
  hue += tuning.hueShift / 360.0f;
  const float sourceSaturation = maximum > 0.0001f ? delta / maximum : 0.0f;
  // Palette stops own chroma. Older pattern code supplied a second, hard-coded
  // saturation multiplier here, which unintentionally washed out otherwise
  // vivid user-edited colors (Starfield, for example, multiplied by 107/255).
  // Retain the argument for the hot-loop/LUT API but make Global saturation
  // the single explicit saturation transform shared by every pattern.
  (void)saturation;
  const float tunedSaturation = constrain(
    sourceSaturation * (tuning.saturation / 100.0f),
    0.0f, 1.0f);
  return makeColor(hue, tunedSaturation,
                   maximum * (value / 255.0f), brightness);
}

void ensureHuePalette(uint8_t saturation, uint8_t value, int brightness) {
  brightness = constrain(brightness, 0, 15);
  const ColorParams& controls = colorParams[(int)currentAnimation];
  if (huePaletteAnimation == (int)currentAnimation &&
      huePaletteBrightness == brightness &&
      huePaletteSaturation == saturation && huePaletteValue == value &&
      huePaletteColorParams.hue == controls.hue &&
      huePaletteColorParams.spread == controls.spread &&
      huePaletteColorParams.palette == controls.palette &&
      huePaletteTuningParams.hueShift == colorTuning[(int)currentAnimation].hueShift &&
      huePaletteTuningParams.saturation == colorTuning[(int)currentAnimation].saturation) return;

  for (int i = 0; i < 256; i++) {
    huePalette[i] = makePaletteColor(i / 256.0f, saturation, value,
                                     brightness, controls);
  }
  huePaletteAnimation = (int)currentAnimation;
  huePaletteBrightness = brightness;
  huePaletteSaturation = saturation;
  huePaletteValue = value;
  huePaletteColorParams = controls;
  huePaletteTuningParams = colorTuning[(int)currentAnimation];
}

inline uint16_t huePaletteColor(float hue) {
  // Conversion to uint8_t intentionally wraps the hue into the 256-entry LUT.
  return huePalette[(uint8_t)((int32_t)(hue * 256.0f))];
}

inline uint16_t scaleRgb565(uint16_t color, uint8_t scale) {
  if (scale >= 255) return color;
  const uint16_t red = (((color >> 11) & 0x1F) * scale + 127) / 255;
  const uint16_t green = (((color >> 5) & 0x3F) * scale + 127) / 255;
  const uint16_t blue = ((color & 0x1F) * scale + 127) / 255;
  return (red << 11) | (green << 5) | blue;
}

uint16_t makeWhite(float value, int brightness) {
  return makeColor(0.0f, 0.0f, value, brightness);
}

uint16_t makeColor(float hue, float saturation, float value, int brightness) {
  brightness = constrain(brightness, 0, 15);
  saturation = constrain(saturation, 0.0f, 1.0f);
  value = constrain(value, 0.0f, 1.0f);

  hue = hue - floorf(hue); // normalize safely to 0.0 <= hue < 1.0

  // Convert UI brightness 0..15 to the full 8-bit RGB range.
  // brightness=15 and value=1.0 can therefore reach 255 on the dominant channel.
  float ui = brightness / 15.0f;
  float cap = 255.0f * ui * value;

  float r = 1.0f, g = 1.0f, b = 1.0f;
  if (saturation > 0.0001f) {
    float h = hue * 6.0f;
    int sector = (int)floorf(h);
    float f = h - sector;
    float p = 1.0f - saturation;
    float q = 1.0f - saturation * f;
    float t = 1.0f - saturation * (1.0f - f);

    switch (sector % 6) {
      case 0: r = 1.0f; g = t;    b = p;    break;
      case 1: r = q;    g = 1.0f; b = p;    break;
      case 2: r = p;    g = 1.0f; b = t;    break;
      case 3: r = p;    g = q;    b = 1.0f; break;
      case 4: r = t;    g = p;    b = 1.0f; break;
      default:r = 1.0f; g = p;    b = q;    break;
    }
  }

  uint8_t rr = (uint8_t)constrain((int)roundf(r * cap), 0, 255);
  uint8_t gg = (uint8_t)constrain((int)roundf(g * cap), 0, 255);
  uint8_t bb = (uint8_t)constrain((int)roundf(b * cap), 0, 255);
  if(photoTransferActive()){
    rr=(uint8_t)roundf(photoTransfer[(uint8_t)roundf(r*value*255.f)]*ui);
    gg=(uint8_t)roundf(photoTransfer[(uint8_t)roundf(g*value*255.f)]*ui);
    bb=(uint8_t)roundf(photoTransfer[(uint8_t)roundf(b*value*255.f)]*ui);
  }
  return matrix.color565(rr, gg, bb);
}

void resyncDisplay() {
  // Never clear/show a black framebuffer here: it can leave the display black
  // if a web request blocks immediately afterward.
  Serial.println("Refreshing display frame timing (no framebuffer clear)...");
  const unsigned long frameDuration =
    (targetFPS > 0) ? (1000000UL / (unsigned long)targetFPS) : 1000000UL;
  const unsigned long now = micros();
  lastFrameTime = now - frameDuration;
  nextFrameDueTime = now; // force a fresh frame on next loop
}

// -----------------------------------------------------------------------------
// Wi-Fi storage and fallback access point
// -----------------------------------------------------------------------------
bool forceSetupAccessPoint() {
  preferences.begin("wifi-list", true);
  const bool enabled = preferences.getBool("force_ap", false);
  preferences.end();
  return enabled;
}

void setForceSetupAccessPoint(bool enabled) {
  preferences.begin("wifi-list", false);
  preferences.putBool("force_ap", enabled);
  preferences.end();
}

bool readWifiNetwork(uint8_t index, String& ssid, String& password) {
  preferences.begin("wifi-list", true);
  const int count = preferences.getInt("count", 0);
  if (index >= count) {
    preferences.end();
    return false;
  }
  ssid = preferences.getString(("ssid_" + String(index)).c_str(), "");
  password = preferences.getString(("pass_" + String(index)).c_str(), "");
  preferences.end();
  return ssid.length() > 0;
}

bool addOrUpdateNetwork(const String& ssid, const String& password) {
  if (ssid.length() == 0 || ssid.length() > 32 || password.length() > 64) return false;

  if (!preferences.begin("wifi-list", false)) return false;
  int count = preferences.getInt("count", 0);
  count = constrain(count, 0, (int)MAX_WIFI_NETWORKS);

  for (int i = 0; i < count; i++) {
    if (preferences.getString(("ssid_" + String(i)).c_str(), "") == ssid) {
      preferences.putString(("pass_" + String(i)).c_str(), password);
      preferences.putBool("force_ap", false);
      const String key="pass_"+String(i);
      bool ok=preferences.isKey(key.c_str())&&preferences.getString(key.c_str())==password&&!preferences.getBool("force_ap",true);
      preferences.end();
      return ok;
    }
  }

  if (count >= MAX_WIFI_NETWORKS) {
    preferences.end();
    return false;
  }

  preferences.putString(("ssid_" + String(count)).c_str(), ssid);
  preferences.putString(("pass_" + String(count)).c_str(), password);
  bool ok=preferences.isKey(("ssid_"+String(count)).c_str())&&preferences.getString(("ssid_"+String(count)).c_str())==ssid&&
    preferences.isKey(("pass_"+String(count)).c_str())&&preferences.getString(("pass_"+String(count)).c_str())==password;
  if(ok)preferences.putInt("count", count + 1);
  preferences.putBool("force_ap", false);
  ok=ok&&preferences.getInt("count",0)==count+1&&!preferences.getBool("force_ap",true);
  preferences.end();
  return ok;
}

bool tryWifiConnection(const String& ssid, const String& password) {
  if (ssid.length() == 0) return false;

  Serial.print("Trying Wi-Fi: ");
  Serial.println(ssid);

  WiFi.disconnect(false, false);
  delay(100);
  WiFi.begin(ssid.c_str(), password.c_str());

  const unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < WIFI_CONNECT_TIMEOUT_MS) {
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected. IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("Connection failed.");
  return false;
}

void startMdns() {
  if (mdnsStarted) return;
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3",
               "pool.ntp.org", "time.nist.gov");
  if (MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    mdnsStarted = true;
    Serial.print("mDNS: http://");
    Serial.print(HOSTNAME);
    Serial.println(".local");
  } else {
    Serial.println("Could not start mDNS.");
  }
}

bool connectToKnownWifi() {
  isAPMode = false;
  WiFi.mode(WIFI_MODE_NULL);
  delay(100);
  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
#if defined(MATRIX_HUB75_DMA)
  WiFi.setSleep(false);
#endif
  WiFi.setAutoReconnect(true);

  preferences.begin("wifi-list", true);
  int count = preferences.getInt("count", 0);
  int preferred = preferences.getInt("preferred", -1);
  preferences.end();
  count = constrain(count, 0, (int)MAX_WIFI_NETWORKS);

  // Selected network first; all others remain available as fallback.
  for (int attempt = -1; attempt < count; attempt++) {
    int i=attempt<0?preferred:attempt;
    if(i<0||i>=count||(attempt>=0&&i==preferred))continue;
    String ssid;
    String password;
    if (readWifiNetwork(i, ssid, password) && tryWifiConnection(ssid, password)) {
      startMdns();
      return true;
    }
  }

  if (strlen(DEFAULT_WIFI_SSID) > 0) {
    if (tryWifiConnection(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASSWORD)) {
      addOrUpdateNetwork(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASSWORD);
      startMdns();
      return true;
    }
  }

  return false;
}

void startAccessPoint() {
  if (isAPMode) return;

  if (mdnsStarted) {
    MDNS.end();
    mdnsStarted = false;
  }

  // Initialize the radio as a station first. This is the same path used by a
  // successful scan and avoids the ESP32-S3 AP transition failure observed
  // after a station connection times out. softAP() then adds the AP interface.
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false, false);
  const bool stationModeStarted = WiFi.mode(WIFI_STA);
  delay(150);
  bool started = stationModeStarted &&
                 WiFi.softAP(SETUP_AP_SSID, nullptr, 1, false, 4);

  if (!started) {
    // Native IDF fallback: retain Arduino's initialized AP netif/DHCP server,
    // but configure the radio directly if WiFi.softAP() rejects the change.
    const bool apModeStarted = WiFi.mode(WIFI_AP);
    wifi_config_t config = {};
    strlcpy((char*)config.ap.ssid, SETUP_AP_SSID,
            sizeof(config.ap.ssid));
    config.ap.ssid_len = strlen(SETUP_AP_SSID);
    config.ap.channel = 1;
    config.ap.authmode = WIFI_AUTH_OPEN;
    config.ap.ssid_hidden = false;
    config.ap.max_connection = 4;
    config.ap.beacon_interval = 100;
    const esp_err_t configResult = apModeStarted
      ? esp_wifi_set_config(WIFI_IF_AP, &config)
      : ESP_FAIL;
    started = apModeStarted && configResult == ESP_OK;
    Serial.printf("Setup AP fallback: mode=%d config=%s\n",
                  apModeStarted ? 1 : 0, esp_err_to_name(configResult));
  }
  isAPMode = started;
  wifiDisconnectedSince = 0;

  if (!started) {
    Serial.println("ERROR: setup access point could not be started.");
    return;
  }

  dnsServer.stop();
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  Serial.println("\nNo known Wi-Fi could be reached.");
  Serial.print("Setup network: ");
  Serial.println(SETUP_AP_SSID);
  Serial.print("Setup page: http://");
  Serial.println(WiFi.softAPIP());
}

void maintainWifi() {
  if (isAPMode) return;

  if (WiFi.status() == WL_CONNECTED) {
    wifiDisconnectedSince = 0;
    return;
  }

  if (wifiDisconnectedSince == 0) wifiDisconnectedSince = millis();

  if (millis() - wifiDisconnectedSince >= WIFI_LOST_TO_AP_MS) {
    Serial.println("Wi-Fi was unavailable for 60 seconds; opening setup access point.");
    startAccessPoint();
  }
}


// -----------------------------------------------------------------------------
// Embedded web interfaces
// -----------------------------------------------------------------------------
const char DASHBOARD_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en" data-theme="field">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#e4e3db">
<title>Matrix.hub by Televator</title>
<script>try{const t=localStorage.getItem('matrix-studio-theme');if(['field','graphite','signal'].includes(t))document.documentElement.dataset.theme=t}catch(e){}</script>
<style>
:root{color-scheme:light;--bg:#e4e3db;--panel:#eeede6;--input:#faf9f3;--line:#c2c3ba;--text:#272d2e;--muted:#656c68;--a:#ec5628;--b:#176b65;--accentInk:#fff;--screen:#242c2d;--screenInk:#ddf0b0;--good:#28784c;--warn:#9a5b00;--bad:#b52b35;--knob:#d4d6cc;--shadow:0 2px 0 #c2c3ba;--mono:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace}
:root[data-theme=graphite]{color-scheme:dark;--bg:#1b201f;--panel:#292f2d;--input:#343c38;--line:#48524b;--text:#e4e9dc;--muted:#a4afa2;--a:#d2ed72;--b:#8cd3bd;--accentInk:#1b231a;--screen:#101712;--screenInk:#d5ef90;--good:#9dde9c;--warn:#edbc76;--bad:#ff9696;--knob:#3c4840;--shadow:0 2px 0 #111712}
:root[data-theme=signal]{color-scheme:light;--bg:#dfbf32;--panel:#f0d247;--input:#f8e38b;--line:#b9a036;--text:#202c32;--muted:#525d51;--a:#233b46;--b:#a13c22;--accentInk:#fff2ac;--screen:#23343a;--screenInk:#f6e997;--good:#245e38;--warn:#845100;--bad:#a1242c;--knob:#e0be36;--shadow:0 2px 0 #b9a036}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:12px/1.35 Arial,Helvetica,system-ui,sans-serif}button,input,select{font:inherit}button,a,input,select,summary{-webkit-tap-highlight-color:transparent}button{cursor:pointer}button:disabled{opacity:.45;cursor:not-allowed}button:focus-visible,input:focus-visible,select:focus-visible,summary:focus-visible,[role=slider]:focus-visible{outline:2px solid var(--a);outline-offset:3px}::selection{background:var(--a);color:var(--accentInk)}
.shell{max-width:1440px;padding:18px 20px 24px;margin:auto}.top{display:flex;justify-content:space-between;align-items:center;gap:16px;padding:0 0 14px;border-bottom:2px solid var(--text);margin-bottom:14px}.brand{display:flex;align-items:center;gap:12px}.logo{background:var(--text);color:var(--panel);width:38px;height:38px;display:grid;place-items:center;border-radius:3px}.matrixIcon{display:grid;grid-template-columns:repeat(4,3px);gap:3px}.matrixIcon i{height:3px;width:3px;background:var(--panel);border-radius:50%}.title h1{margin:0;font-size:20px;letter-spacing:-.8px;font-weight:750}.title p{margin:2px 0 0;font:10px var(--mono);color:var(--muted)}.topRight{display:flex;align-items:center;gap:16px}.themeSwitch{display:flex;gap:4px}.themeSwitch button{padding:6px 8px;border:1px solid var(--line);border-radius:3px;background:var(--panel);color:var(--muted);font:10px var(--mono)}.themeSwitch button[aria-pressed=true]{border-color:var(--text);color:var(--text);box-shadow:inset 0 -2px var(--a)}.themeSwatch{display:inline-block;width:7px;height:7px;margin-right:5px;border:1px solid #62675c;border-radius:50%}.status{display:grid;gap:3px;justify-items:end;font:9px var(--mono)}.badge{color:var(--muted);font:9px var(--mono);white-space:nowrap}.dot{display:inline-block;width:6px;height:6px;background:var(--good);border-radius:50%;margin-right:5px}
.grid{display:grid;grid-template-columns:280px minmax(420px,1fr) 228px;gap:12px;align-items:start}.card{background:var(--panel);border:1px solid var(--line);border-radius:5px;box-shadow:var(--shadow);overflow:hidden}.cardHead{padding:11px 12px;display:flex;justify-content:space-between;align-items:center;gap:8px;border-bottom:1px solid var(--line)}.cardHead h2{margin:0;font:600 11px var(--mono);letter-spacing:.4px;text-transform:uppercase}.sectionNumber{font:10px var(--mono);color:var(--a);margin-right:7px}.cardBody{padding:12px}.sectionTitle{display:flex;align-items:center;justify-content:space-between;gap:8px;font:10px var(--mono);text-transform:uppercase;letter-spacing:.7px;color:var(--muted);margin:0 0 9px}.sectionTitle small{font-size:9px;letter-spacing:0;text-transform:none}.stack{display:grid;gap:12px}
input[type=text],input[type=password],input[type=number],input[type=time],input[type=search],select{width:100%;min-width:0;height:31px;border:1px solid var(--line);border-radius:3px;background:var(--input);color:var(--text);padding:5px 7px;font:11px var(--mono)}input[type=range]{width:100%;min-width:0;accent-color:var(--a);margin:0;height:24px}input[type=search]{font-family:Arial,Helvetica,sans-serif}label{cursor:inherit}
.patternToolbar{display:grid;grid-template-columns:1fr 32px;gap:6px;margin-bottom:7px}.libraryFilters{display:flex;gap:6px;align-items:center;margin-bottom:10px}.filterButton{background:none;border:1px solid var(--line);border-radius:3px;color:var(--muted);font:10px var(--mono);padding:5px 7px}.filterButton[aria-pressed=true]{background:var(--text);color:var(--panel);border-color:var(--text)}.libraryCount{margin-left:auto;font:9px var(--mono);color:var(--muted)}.patternGallery{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:6px;max-height:calc(100dvh - 260px);min-height:220px;overflow:auto;align-content:start;padding:0 3px 3px 0;scrollbar-width:thin;scrollbar-color:var(--line) transparent}.patternCard{position:relative;min-width:0;background:var(--input);border:1px solid var(--line);border-radius:3px;overflow:hidden}.patternCard.selected{border-color:var(--a);box-shadow:inset 0 -3px var(--a)}.patternCard:hover{border-color:var(--text)}.patternSelect{display:block;width:100%;border:0;padding:0;background:none;text-align:left;color:var(--text)}.patternCard canvas{display:block;width:100%;aspect-ratio:1;background:#020405;image-rendering:pixelated}.patternInfo{padding:5px 5px 7px}.patternInfo strong{font-size:9px;line-height:1.15;display:-webkit-box;-webkit-line-clamp:2;-webkit-box-orient:vertical;overflow:hidden;min-height:21px}.patternInfo span{display:none}.bookmark{position:absolute;top:3px;right:3px;border:1px solid #6a7468;border-radius:2px;background:#17201dde;color:#eceddd;width:25px;height:25px;padding:0;font-size:19px;line-height:23px}.bookmark.marked{color:#ffe282;border-color:#ffe282}.emptyLibrary{grid-column:1/-1;padding:25px 5px;font:11px var(--mono);color:var(--muted);text-align:center}
.lcd{display:grid;grid-template-columns:76px minmax(0,1fr);gap:14px;padding:12px;margin-bottom:12px;border-radius:3px;background:var(--screen);color:var(--screenInk)}.lcd canvas{width:76px;height:76px;image-rendering:pixelated;background:#000;border:1px solid #566152}.lcdContent{min-width:0;display:flex;flex-direction:column;justify-content:space-between;gap:4px}.lcdEyebrow{font:9px var(--mono);text-transform:uppercase;letter-spacing:1.2px;opacity:.65}.lcd h2{font:500 clamp(16px,1.5vw,22px)/1.1 var(--mono);letter-spacing:-.8px;margin:0;overflow-wrap:anywhere}.lcdMeta{font:9px var(--mono);opacity:.65}.lcdReadouts{display:flex;gap:18px;font:10px var(--mono)}.lcdReadouts b{font-size:13px;font-weight:500}.liveLed{display:inline-block;width:5px;height:5px;background:var(--screenInk);border-radius:50%;margin-right:5px}
.controls{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:7px}.control{min-width:0;border:1px solid var(--line);border-radius:3px;padding:8px;background:var(--input)}.controlTop{display:flex;align-items:start;justify-content:space-between;gap:3px;margin-bottom:5px}.control label{font-size:10px;font-weight:600;line-height:1.2;min-height:24px}.control .value{display:none}.control .help{display:none}.controlNumber{display:block;font:8px var(--mono);color:var(--muted);margin-bottom:3px}.rangeRow{display:grid;grid-template-columns:minmax(0,1fr) 45px;gap:4px;align-items:center}.miniInput{padding:4px!important;text-align:right;height:26px!important;font-size:10px!important}.encoderRow{display:flex;justify-content:center;align-items:center;height:37px;margin-bottom:3px}.encoder{width:32px;height:32px;border-radius:50%;border:1px solid var(--line);background:var(--knob);box-shadow:inset 0 1px 0 #ffffff35,0 2px 0 var(--line);position:relative;touch-action:none;cursor:ns-resize}.encoder:after{content:"";position:absolute;inset:-4px;border:1px dotted var(--line);border-bottom-color:transparent;border-radius:50%;pointer-events:none}.needle{position:absolute;inset:0;transform:rotate(var(--angle,-135deg));pointer-events:none}.needle:after{content:"";display:block;position:absolute;top:4px;left:14px;height:9px;width:2px;background:var(--a);border-radius:1px}.controls .control:nth-child(1) .encoder{background:var(--a)}.controls .control:nth-child(1) .needle:after{background:var(--accentInk)}
.editorSection{border-top:1px solid var(--line);padding-top:10px;margin-top:12px}.colorControls{grid-template-columns:repeat(3,minmax(0,1fr));gap:6px}.colorControls .control label{min-height:12px}.colorControls .encoderRow{display:none}.colorControls .rangeRow{grid-template-columns:minmax(0,1fr) 45px}.gradientPreview{height:12px;border:1px solid var(--line);border-radius:2px;margin:4px 0}.colorStops{display:grid;grid-template-columns:repeat(4,1fr);gap:3px}.colorStops input{width:100%;height:24px;min-width:0;padding:1px;border:1px solid var(--line);border-radius:2px;background:var(--panel);cursor:pointer}.lfoGrid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:6px}.lfoGrid .control label{min-height:12px}.lfoGrid .encoderRow{display:none}.editorBottom{display:flex;justify-content:space-between;align-items:center;gap:8px;margin-top:12px}.hint{font:9px var(--mono);color:var(--muted)}.help{font-size:10px;color:var(--muted);line-height:1.45;margin:8px 0 0}.qualityRow{display:flex;align-items:center;gap:9px;margin:10px 0 0;font:10px var(--mono);color:var(--muted)}.qualityRow select{flex:1;height:27px;font-size:10px}.patchGrid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:6px}.patch{border:1px solid var(--line);border-radius:3px;padding:7px;min-width:0}.patch strong{display:flex;align-items:center;justify-content:space-between;font:10px var(--mono)}.defaultMark{width:5px;height:5px;border-radius:50%;background:var(--b);opacity:0}.patch.isDefault .defaultMark{opacity:1}.buttonRow{display:grid;grid-template-columns:1fr 1fr;gap:4px;margin-top:6px}.btn{display:inline-block;background:var(--text);color:var(--panel);border:1px solid transparent;border-radius:3px;padding:7px 9px;font:600 10px var(--mono);text-align:center;text-decoration:none;line-height:1.15;cursor:pointer;min-height:30px}.btn:hover{filter:brightness(.95)}.btn:active{transform:translateY(1px)}.primary,.teal{background:var(--a);color:var(--accentInk)}.ghost{border-color:var(--line);background:var(--input);color:var(--text)}.danger{border-color:var(--bad);background:none;color:var(--bad)}.wide{width:100%}.patch .btn{padding:5px;min-height:25px;font-size:9px}.iconButton{padding:4px;font-size:17px}.settingsGrid{display:grid;gap:10px}.setting{min-width:0;display:grid;gap:5px}.settingHeader{display:flex;justify-content:space-between;align-items:center;gap:5px}.settingHeader label{font-size:11px;font-weight:600}.settingHeader span{font:9px var(--mono);color:var(--muted)}.two{display:grid;grid-template-columns:1fr 1fr;gap:7px}.transportReadout{display:flex;justify-content:space-between;align-items:center;padding:10px;background:var(--input);border:1px solid var(--line);border-radius:3px}.transportReadout>span{font:10px var(--mono);text-transform:uppercase;letter-spacing:.6px}.toggle{width:34px;height:21px;display:inline-block;position:relative}.toggle input{position:absolute;inset:0;opacity:0;margin:0;cursor:pointer;width:100%;height:100%;z-index:1}.toggle b{position:absolute;inset:0;border:1px solid var(--line);border-radius:3px;background:var(--knob)}.toggle b:after{content:"";position:absolute;width:13px;height:13px;left:3px;top:3px;background:var(--muted);border-radius:1px;transition:transform .15s}.toggle input:checked+b{background:var(--a);border-color:var(--a)}.toggle input:checked+b:after{background:var(--accentInk);transform:translateX(13px)}.toggle input:focus-visible+b{outline:2px solid var(--text);outline-offset:3px}
.fold{border-top:1px solid var(--line);margin-top:11px;padding-top:10px}.fold summary{cursor:pointer;display:flex;align-items:center;justify-content:space-between;gap:8px;list-style:none;font:10px var(--mono);text-transform:uppercase;letter-spacing:.4px}.fold summary::-webkit-details-marker{display:none}.fold summary:after{content:"+";font-size:16px;line-height:14px;color:var(--muted)}.fold[open] summary:after{content:"−"}.foldBody{padding-top:11px}.diagGrid{display:grid;grid-template-columns:1fr 1fr;gap:5px}.metric{background:var(--input);border:1px solid var(--line);border-radius:3px;padding:7px;min-width:0}.metric span{display:block;font-size:9px;color:var(--muted);margin-bottom:4px}.metric strong{font:11px var(--mono);overflow-wrap:anywhere}.wideMetric{grid-column:1/-1}.diagGood{color:var(--good)}.diagWarn{color:var(--warn)}.diagBad{color:var(--bad)}.networkInfo{font:10px/1.6 var(--mono);color:var(--muted);overflow-wrap:anywhere;margin-bottom:10px}.networkInfo strong{color:var(--text)}.utilityButtons{display:grid;gap:6px}.panelFooter{display:flex;justify-content:space-between;gap:8px;margin-top:15px;border-top:1px solid var(--line);padding-top:8px;font:9px var(--mono);color:var(--muted)}.toast{position:fixed;bottom:18px;right:18px;max-width:min(380px,calc(100% - 36px));padding:11px 14px;border-radius:3px;background:var(--screen);color:var(--screenInk);border:1px solid var(--line);font:11px var(--mono);opacity:0;transform:translateY(15px);pointer-events:none;transition:.2s;z-index:10}.toast.show{opacity:1;transform:none}.toast.bad{border-color:var(--bad)}.toast.ok{border-color:var(--b)}
.editor{container-type:inline-size}.lcd{grid-template-columns:64px minmax(0,1fr);padding:10px}.lcd canvas{width:64px;height:64px}.controlNumber{display:inline-block;margin:0 5px 0 0}.controls .control{padding:6px}.controlTop{margin-bottom:3px}.encoderRow{height:32px;margin-bottom:2px}.miniInput{height:24px!important}.colorControls .control label,.lfoGrid .control label{min-height:12px}.patch strong{margin-bottom:5px}.patch .buttonRow{grid-template-columns:repeat(3,1fr);margin-top:0}.patch .btn{min-height:24px}.patchGrid .patch{padding:6px}
@container(min-width:650px){.colorControls{grid-template-columns:1fr 1.4fr repeat(4,1fr)}}
@media(min-width:1101px) and (max-width:1250px){.grid{grid-template-columns:242px minmax(420px,1fr) 208px}.shell{padding:14px}.cardBody{padding:10px}.controls{gap:5px}.control{padding:6px}.rangeRow{grid-template-columns:minmax(0,1fr) 40px}}
@media(max-width:1100px){.grid{grid-template-columns:250px minmax(0,1fr)}.transport{grid-column:2}.library{grid-row:1/3}.patternGallery{max-height:calc(100dvh - 200px)}.controls{grid-template-columns:repeat(4,minmax(0,1fr))}.topRight{gap:10px}.status{display:none}.transport .cardBody{display:grid;grid-template-columns:1fr 1fr;gap:12px}.transportMain{min-width:0}.systemFolds{min-width:0}.systemFolds .fold:first-child{margin-top:0;border-top:0;padding-top:0}}
@media(max-width:760px){.shell{padding:12px}.top{gap:8px;align-items:start}.title h1{font-size:17px}.title p{font-size:9px;max-width:155px}.logo{display:none}.topRight{display:block}.themeSwitch button{padding:7px 8px;font-size:9px}.themeSwatch{display:none}.grid{display:flex;flex-direction:column;gap:12px}.editor,.library,.transport{width:100%}.editor{order:1}.library{order:2}.transport{order:3}.patternGallery{grid-template-columns:repeat(5,minmax(0,1fr));max-height:320px;min-height:0}.patternInfo strong{font-size:9px}.lcd h2{font-size:20px}.controls{grid-template-columns:repeat(4,minmax(0,1fr))}.control{padding:6px}.rangeRow{grid-template-columns:1fr}.miniInput{height:28px!important}.controls .control label{font-size:10px}.colorControls .rangeRow,.lfoGrid .rangeRow{grid-template-columns:1fr 42px}.encoder{width:34px;height:34px}.needle:after{left:15px}.hint{max-width:135px;font-size:8px}.panelFooter{flex-wrap:wrap}.lcdReadouts{gap:12px}}
@media(max-width:430px){.title h1{font-size:14px}.themeSwitch button{padding:7px 5px}.cardBody{padding:9px}.controls{gap:4px}.colorControls{grid-template-columns:repeat(2,minmax(0,1fr))}.patternGallery{grid-template-columns:repeat(4,minmax(0,1fr))}.transport .cardBody{display:block}.systemFolds{margin-top:12px}.lcd{gap:10px;padding:10px}.editorBottom .btn{font-size:9px;padding:7px}.lfoGrid .control{padding:5px}.lfoGrid .rangeRow{grid-template-columns:1fr}.hint{display:none}.editorBottom{justify-content:flex-end}}
@media(prefers-reduced-motion:reduce){*{scroll-behavior:auto!important;transition:none!important}}

.control.keyboardSelected{outline:1px solid var(--a);outline-offset:-1px}.control.keyboardSelected .controlTop label:after{content:" ↔";color:var(--a)}
.organizeFilters{display:grid;grid-template-columns:1fr 1fr;gap:5px;margin-bottom:7px}.organizeFilters select{min-width:0;font-size:10px;padding:5px}.groupToggle{display:flex;align-items:center;gap:5px;font:10px var(--mono);margin-bottom:8px}.groupToggle input{width:auto}.familyHeading{grid-column:1/-1;font:10px var(--mono);color:var(--muted);padding:8px 2px 2px;border-top:1px solid var(--line)}.patternRating{display:block;padding:0 5px 6px;font:10px var(--mono);color:var(--a)}.libraryEditor{border-bottom:1px solid var(--line);padding:8px 0;margin-bottom:9px}.ratingRow{display:flex;align-items:center;gap:3px}.ratingRow button{background:none;border:0;color:var(--muted);font-size:22px;padding:1px 3px;cursor:pointer}.ratingRow button[aria-pressed=true]{color:var(--a)}.ratingRow .clearRating{font:10px var(--mono);margin-left:5px}.organizationFields{display:grid;grid-template-columns:1fr 1fr;gap:7px;margin:8px 0}.organizationFields label{font:10px var(--mono);min-width:0}.organizationFields input{width:100%;margin-top:4px;font-size:11px}.organizationFields .wide{grid-column:1/-1}.libraryEditor summary{font:10px var(--mono);cursor:pointer;margin-top:5px}.organizationActions{display:flex;gap:8px;align-items:center}.organizationActions small{font:9px var(--mono);color:var(--muted)}
</style>
</head>
<body>
<div class="shell">
<header class="top">
  <div class="brand"><div class="logo" aria-hidden="true"><span class="matrixIcon"><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i></span></div><div class="title"><h1 id="deviceTitle">Matrix.hub</h1><p id="deviceSubtitle">by Televator / connecting…</p></div></div>
  <div class="topRight"><div class="themeSwitch" role="group" aria-label="Color theme"><button type="button" data-theme-choice="field" onclick="setTheme('field')" aria-pressed="true"><i class="themeSwatch" style="background:#eeede6"></i>01 Field</button><button type="button" data-theme-choice="graphite" onclick="setTheme('graphite')" aria-pressed="false"><i class="themeSwatch" style="background:#292f2d"></i>02 Graphite</button><button type="button" data-theme-choice="signal" onclick="setTheme('signal')" aria-pressed="false"><i class="themeSwatch" style="background:#f0d247"></i>03 Signal</button></div><div class="status"><span class="badge"><i class="dot"></i><span id="modeBadge">Connecting</span></span><span class="badge" id="ipBadge">—</span></div></div>
</header>
<main class="grid">
<section class="card library">
  <div class="cardHead"><h2><span class="sectionNumber">01</span>Library</h2><span id="catalogCount" class="badge">59 patterns</span></div>
  <div class="cardBody">
    <div class="patternToolbar"><input id="patternSearch" type="search" aria-label="Search patterns" placeholder="Find a pattern…" oninput="renderPatternGallery()"><button class="btn ghost iconButton" title="Refresh device state" aria-label="Refresh device state" onclick="refreshState()">↻</button></div>
    <div class="libraryFilters"><button type="button" id="allFilter" class="filterButton" aria-pressed="true" onclick="setLibraryFilter(false)">All</button><button type="button" id="starFilter" class="filterButton" aria-pressed="false" onclick="setLibraryFilter(true)">⚑ Bookmarks</button><span id="libraryCount" class="libraryCount">—</span></div>
    <div class="organizeFilters"><select id="folderFilter" aria-label="Filter folder" onchange="renderPatternGallery()"><option value="">All folders</option></select><select id="tagFilter" aria-label="Filter tag" onchange="renderPatternGallery()"><option value="">All tags</option></select><select id="ratingFilter" aria-label="Minimum rating" onchange="renderPatternGallery()"><option value="0">All ratings</option><option value="-1">Not rated</option><option value="1">★ 1+</option><option value="2">★★ 2+</option><option value="3">★★★ 3+</option><option value="4">★★★★ 4+</option><option value="5">★★★★★ 5</option></select><select id="librarySort" aria-label="Sort patterns" onchange="renderPatternGallery()"><option value="catalog">Library order</option><option value="rating">Best rated</option><option value="name">Name A–Z</option></select></div>
    <div class="libraryFilters" style="flex-wrap:wrap"><label class="groupToggle" style="margin:0;flex:1;min-width:120px"><input id="groupFamilies" type="checkbox" checked onchange="renderPatternGallery()">Keep variants together</label><select id="archiveView" style="width:auto;flex:1;font:10px var(--mono);padding:5px" aria-label="Library or archive" onchange="renderPatternGallery()"><option value="active">Library</option><option value="archived">Archive / restore</option></select></div>
    <div id="patternGallery" class="patternGallery" aria-label="Pattern library"></div>
  </div>
</section>
<section class="card editor">
  <div class="cardHead"><h2><span class="sectionNumber">02</span>Pattern / edit</h2><span class="badge">8 encoders</span></div>
  <div class="cardBody">
    <div class="lcd"><canvas id="activePreview" width="28" height="28" aria-label="Selected pattern preview"></canvas><div class="lcdContent"><div class="lcdEyebrow"><i class="liveLed"></i>Now playing</div><h2 id="activePatternName">Connecting…</h2><div id="activePatternSource" class="lcdMeta">Waiting for the matrix</div><div class="lcdReadouts"><span><b id="liveFPS">—</b> fps</span><span><b id="liveRefresh">—</b> Hz scan</span><span id="liveResolution">—</span></div></div></div>
    <div class="libraryEditor" id="libraryEditor"><div class="ratingRow" id="patternRating" role="group" aria-label="Rate this pattern"></div><details><summary>Organize / folder · variants · tags</summary><div class="organizationFields"><label>Folder<input id="metaFolder" list="folderNames" maxlength="24" placeholder="Unfiled" oninput="editLibraryDraft('folder',this.value)"></label><label>Variant family<input id="metaFamily" list="familyNames" maxlength="48" placeholder="Automatic grouping" oninput="editLibraryDraft('family',this.value)"></label><label class="wide">Tags · comma separated · up to 4<input id="metaTags" maxlength="90" placeholder="acid, geometric, calm" oninput="editLibraryDraft('tags',this.value)"></label></div><datalist id="folderNames"></datalist><datalist id="familyNames"></datalist></details><div class="organizationActions"><button class="btn ghost" type="button" onclick="saveOrganization()">Save rating / organization</button><button class="filterButton" type="button" onclick="resetOrganizationDraft()">Revert</button><small id="organizationStatus">Saved on matrix</small></div></div>
    <div class="sectionTitle"><span>Parameters</span><small>drag dial · shift = fine</small></div>
    <div id="paramControls" class="controls"></div>
    <div id="qualityControl" class="qualityRow"><label for="patternQuality">Render</label><select id="patternQuality" title="Native calculates every LED pixel. Smooth fast interpolates a smaller grid." onchange="command('setPatternQuality',{value:Number(this.value)})"><option value="0">Native / maximum detail</option><option value="1">Smooth / faster motion</option></select></div>
    <div id="colorBox" class="editorSection"><div class="sectionTitle"><span>Color / palette</span><small>saved with pattern</small></div><div id="colorControls" class="controls colorControls"></div></div>
    <div class="editorSection"><div class="sectionTitle"><span>LFO / modulation</span><small>sine wave</small></div><div id="lfoControls" class="lfoGrid"></div></div>
    <div class="editorSection"><div class="sectionTitle"><span>Memory / patches</span><small>● auto-cycle default</small></div><div id="patches" class="patchGrid"></div></div>
    <div class="editorBottom"><span class="hint">Changes send on release.<br>Arrows: step · Shift+arrows: 10×.<br>Save to keep your settings.</span><button class="btn primary" onclick="savePatternDefault()">Save pattern default ↗</button></div>
  </div>
</section>
<aside class="card transport">
  <div class="cardHead"><h2><span class="sectionNumber">03</span>Playback / system</h2></div>
  <div class="cardBody">
    <div class="transportMain settingsGrid">
      <div class="transportReadout"><span>Auto-cycle</span><label class="toggle" title="Auto-cycle playback"><input id="autoCycle" type="checkbox" aria-label="Auto-cycle"><b></b></label></div>
      <div class="two"><div class="setting"><div class="settingHeader"><label for="cycleInterval">Interval</label><span>s</span></div><input id="cycleInterval" type="number" min="1" max="86400"></div><div class="setting"><div class="settingHeader"><label for="fps">Target</label><span>FPS</span></div><input id="fps" type="number" min="1" max="240"></div></div>
      <details class="fold"><summary>Schedule <span class="badge">Berlin time</span></summary><div class="foldBody settingsGrid">
        <div class="settingHeader"><label>Enabled</label><label class="toggle"><input id="scheduleEnabled" type="checkbox" aria-label="Enable time schedule"><b></b></label></div>
        <div class="two"><div class="setting"><label for="scheduleStart">On at</label><input id="scheduleStart" type="time"></div><div class="setting"><label for="scheduleEnd">Off at</label><input id="scheduleEnd" type="time"></div></div>
        <div class="two"><div class="setting"><label for="scheduleFadeIn">Fade in / s</label><input id="scheduleFadeIn" type="number" min="1" max="3600"></div><div class="setting"><label for="scheduleFadeOut">Fade out / s</label><input id="scheduleFadeOut" type="number" min="1" max="3600"></div></div>
      </div></details>
      <button class="btn primary wide" onclick="savePlayback()">Save playback ↗</button>
    </div>
    <div class="systemFolds">
      <details class="fold"><summary>Live diagnostics</summary><div class="foldBody"><div class="sectionTitle"><span id="diagEngine">—</span></div><div class="diagGrid">
        <div class="metric wideMetric"><span>Pattern</span><strong id="diagPattern">Waiting…</strong></div>
        <div class="metric"><span>Actual / target</span><strong id="diagFPS">—</strong></div><div class="metric"><span id="diagRefreshLabel">Electrical refresh</span><strong id="diagRefresh">—</strong></div>
        <div class="metric"><span>Render budget</span><strong id="diagLoad">—</strong></div><div class="metric"><span>Average render</span><strong id="diagAverage">—</strong></div>
        <div class="metric"><span>Worst render</span><strong id="diagWorst">—</strong></div><div class="metric"><span>Computation</span><strong id="diagDraw">—</strong></div>
        <div class="metric"><span>Output transfer</span><strong id="diagShow">—</strong></div><div class="metric"><span>Over budget</span><strong id="diagOverBudget">—</strong></div>
        <div class="metric wideMetric"><span>Free / minimum heap</span><strong id="diagHeap">—</strong></div>
      </div><p class="help" id="diagStatus">Live readings update once per second.</p></div></details>
      <details class="fold" id="calibrationPanel"><summary>Color calibration / comparison</summary><div class="foldBody settingsGrid">
<p class="help" id="calibrationStatus"></p>
<div class="setting"><label for="calMode">Display view</label><select id="calMode" onchange="command('setCalibrationMode',{value:Number(this.value)})"><option value="0">Normal playback</option><option value="1">Neutral gray + RGB chart</option><option value="2">Chart: RAW left / FIX right</option><option value="3" id="calDepthOption">Chart: simulated 3-bit / native 4-bit</option></select></div>
<div class="two"><div class="setting"><label for="calPhoto">Comparison chart</label><select id="calPhoto" onchange="command('setCalibrationPhoto',{value:Number(this.value)})"><option value="0">Grayscale</option><option value="1">RGB ramps</option></select></div><div class="setting"><label for="calLevel">Test brightness / 255</label><input id="calLevel" type="number" min="32" max="128" onchange="command('setCalibrationLevel',{value:Number(this.value)})"></div></div>
<p class="help">Chart: gray ramp; white / red / green / blue blocks; RGB ramps. Judge the white block with your eyes, not auto-exposed camera images. Reduce excess channels until neutral.</p>
<div class="setting"><label for="calR">Panel red / %</label><input id="calR" type="range" min="0" max="100" onchange="applyColorCalibration()"></div>
<div class="setting"><label for="calG">Panel green / %</label><input id="calG" type="range" min="0" max="100" onchange="applyColorCalibration()"></div>
<div class="setting"><label for="calB">Panel blue / %</label><input id="calB" type="range" min="0" max="100" onchange="applyColorCalibration()"></div>
<div class="two"><div class="setting"><label for="calGamma">Chart gamma / 1.0–2.6</label><input id="calGamma" type="number" min="1" max="2.6" step="0.05" onchange="applyColorCalibration()"></div><div class="setting"><label for="calCorrect">Chart correction</label><input id="calCorrect" type="checkbox" onchange="applyColorCalibration()"></div></div>
<p class="help">RGB balance affects this device, all patterns. Gamma affects comparison charts only; RAW/FIX always compares uncorrected with corrected. No dithering. Tests pause auto-cycle temporarily and expire after 10 minutes. Pattern defaults are untouched.</p>
<button class="btn primary wide" onclick="command('saveColorCalibration')">Save device calibration</button>
<button class="btn ghost wide" onclick="resetColorCalibration()">Neutral RGB / uncorrected chart</button>
</div></details>
<details class="fold"><summary>Utilities</summary><div class="foldBody utilityButtons">
        <div class="setting"><div class="settingHeader"><label for="resyncInterval">Legacy re-sync / s</label></div><input id="resyncInterval" type="number" min="0" max="86400"><span class="help" id="resyncHelp">Leave at 0 for HUB75</span></div>
        <button class="btn ghost wide" onclick="command('resync')">Reset frame timing</button>
        <button id="depthTestButton" class="btn ghost wide" onclick="toggleDepthTest()">3-bit / 4-bit test</button>
        <button id="luaHelloButton" class="btn ghost wide" onclick="toggleLuaHello()">Hello Lua test</button>
      </div></details>
      <details class="fold"><summary>Wi-Fi / recovery</summary><div class="foldBody"><div id="networkInfo" class="networkInfo">Connecting…</div><div class="settingsGrid">
      <label for="savedWifi">Saved networks</label><select id="savedWifi" aria-label="Saved networks"></select>
      <button id="connectWifi" class="btn primary wide" onclick="connectSavedNetwork()">Connect selected</button>
      <p id="wifiConnection" class="help"></p>
      <input id="newSsid" type="text" maxlength="32" aria-label="Network name" placeholder="Network name / SSID"><input id="newPass" type="password" maxlength="64" aria-label="Wi-Fi password" placeholder="Password">
      <button id="saveWifi" class="btn ghost wide" onclick="addNetwork()">Save network</button>
      <p id="wifiMessage" class="help" role="status" aria-live="polite">Save stores credentials. Connect selected restarts the matrix and tries that network first.</p>
      <button class="btn danger wide" onclick="resetWifi()">Forget all networks</button></div></div></details>
    </div>
  </div>
</aside>
</main>
<footer class="panelFooter"><span>MX / visual instrument · v2.0</span><span>Local control / no cloud required</span></footer>
</div>
<div id="toast" class="toast" role="status" aria-live="polite"></div>
<script>
const defs=[
 {name:'Starfield',params:[['b','Brightness',0,15,1,'RGB brightness (full range)'],['as','Animation speed',0.1,3,0.05,'Overall movement multiplier'],['n','Stars',10,300,1,'Number of active stars'],['s','Perspective scale',2,10,0.1,'Field-of-view scale'],['w','Warp speed',0.05,1,0.01,'Forward travel speed']],lfo:['Animation speed','Stars','Perspective scale','Warp speed']},
 {name:'Warped Checkerboard',params:[['b','Brightness',0,15,1,'RGB brightness (full range)'],['as','Animation speed',0.1,3,0.05,'Overall animation rate'],['ws','Wave scale',2,10,0.1,'Spacing of the radial distortion'],['d','Distortion',1,8,0.1,'Warping depth'],['wsp','Wave speed',1,8,0.1,'Distortion travel speed']],lfo:['Animation speed','Wave scale','Distortion','Wave speed']},
 {name:'Moiré Rings',params:[['b','Brightness',0,15,1,'RGB brightness (full range)'],['as','Animation speed',0.1,3,0.05,'Overall animation rate'],['orb','Orbit radius',0,16,0.1,'Movement radius of ring centers'],['rt','Ring thickness',2,16,0.1,'Ring spacing / thickness'],['es','Expansion speed',0.1,5,0.05,'Ring phase speed']],lfo:['Animation speed','Orbit radius','Ring thickness','Expansion speed']},
 {name:'Moiré Lines',params:[['b','Brightness',0,15,1,'RGB brightness (full range)'],['as','Animation speed',0.1,3,0.05,'Overall animation rate'],['rs1','Rotation one',0.1,3,0.05,'First line-field rotation'],['rs2','Rotation two',0.1,3,0.05,'Second line-field rotation'],['lf','Line frequency',0.1,2,0.05,'Density of line fields']],lfo:['Animation speed','Rotation one','Rotation two','Line frequency']},
 {name:'Warping Checkers',params:[['b','Brightness',0,15,1,'RGB brightness (full range)'],['as','Animation speed',0.1,3,0.05,'Overall animation rate'],['os','Orbit speed',0.1,2,0.05,'Motion of distortion center'],['rt','Ring thickness',1,8,0.1,'Band spacing'],['es','Expansion speed',0.1,2,0.05,'Outward phase motion']],lfo:['Animation speed','Orbit speed','Ring thickness','Expansion speed']},
 {name:'Shockwave',params:[['b','Brightness',0,15,1,'RGB brightness (full range)'],['as','Animation speed',0.1,3,0.05,'Overall animation rate'],['ws','Wave speed',0.1,5,0.05,'Radial wave travel speed'],['wt','Wave thickness',0.1,5,0.05,'Radial wavelength']],lfo:['Animation speed','Wave speed','Wave thickness']},
 {name:'Warp Corridors',params:[['b','Brightness',0,15,1,'RGB brightness (full range)'],['as','Animation speed',0.1,3,0.05,'Overall animation rate'],['rs','Rotation speed',0.1,2,0.05,'Corridor rotation'],['ws','Warp speed',0.1,2,0.05,'Frequency breathing rate'],['bf','Band frequency',0.1,2,0.05,'Corridor stripe density']],lfo:['Animation speed','Rotation speed','Warp speed','Band frequency']},
 {name:'Pixel Avalanche',params:[['b','Brightness',0,15,1,'Light-phase output level'],['as','Animation speed',0.1,3,0.05,'Simulation update rate'],['sr','Spawn rate',1,10,1,'New pixels per update']],lfo:['Animation speed','Spawn rate']},
 {name:'Kaleidoscope',params:[['b','Brightness',0,15,1,'RGB brightness (full range)'],['as','Animation speed',0.1,3,0.05,'Overall animation rate'],['p','Points',1,15,1,'Number of source points'],['rs','Rotation speed',0.1,2,0.05,'Pattern rotation'],['es','Expansion speed',0.1,2,0.05,'Radius pulsing speed']],lfo:['Animation speed','Points','Rotation speed','Expansion speed']},
 {name:'Bayer Flow',params:[['b','Brightness',0,15,1,'RGB brightness (full range)'],['as','Animation speed',0.1,3,0.05,'Motion through the ordered threshold field'],['scale','Pattern scale',1,16,1,'Spatial frequency of the flowing field'],['contrast','Contrast',1,8,1,'Threshold contrast and dot density'],['drift','Matrix drift',0.1,3,0.05,'Movement of the 8×8 Bayer threshold matrix']],lfo:['Animation speed','Pattern scale','Contrast','Matrix drift']},
 {name:'Floyd Flow',params:[['b','Brightness',0,15,1,'RGB brightness (full range)'],['as','Animation speed',0.1,3,0.05,'Motion of the source field'],['levels','Levels',2,6,1,'Number of quantized diffusion levels'],['scale','Pattern scale',1,16,1,'Spatial frequency of the source field'],['warp','Warp',0.1,3,0.05,'Strength and speed of the diagonal wave']],lfo:['Animation speed','Levels','Pattern scale','Warp']},
 {name:'Snake',params:[['b','Brightness',0,15,1,'RGB brightness (full range)'],['speed','Moves per second',1,20,0.25,'Retro grid movement speed'],['length','Maximum length',8,300,1,'Snake grows toward this limit by eating apples'],['steering','Apple instinct',0,1,0.01,'How strongly the autonomous snake seeks the apple']],lfo:['Moves per second','Maximum length','Apple instinct']},
 {name:'Squares',params:[['b','Brightness',0,15,1,'RGB brightness (full range)'],['as','Tunnel speed',0.1,4,0.05,'How quickly outlines expand from the center'],['count','Square count',2,20,1,'Number of simultaneous tunnel layers'],['thickness','Line thickness',1,4,1,'Outline weight in pixels'],['spacing','Depth spacing',0.2,2,0.05,'Distance between successive tunnel layers'],['variance','Speed variance',0,0.5,0.01,'Individual speed difference between squares']],lfo:['Tunnel speed','Square count','Depth spacing','Speed variance']},
 {name:'Perspective Flow',params:[['b','Brightness',0,15,1,'Line brightness through the selected palette'],['density','Line density',2.3,5,0.05,'Vertical spacing of the perspective layers'],['amplitude','Wave amplitude',15,60,0.5,'Height of the organic terrain waves'],['as','Flow speed',0.1,5,0.05,'Forward evolution speed'],['detail','Harmonic detail',3,12,1,'Number of sine harmonics in each line']],lfo:['Line density','Wave amplitude','Flow speed','Harmonic detail']},
 {name:'Flow Sphere',params:[['b','Brightness',0,15,1,'Projected sphere brightness through the selected palette'],['speed','Flow speed',0.05,6,0.05,'Surface advection speed; kick and snare gate this by default'],['scale','Field scale',0.5,4,0.05,'Spatial frequency of the three-axis flow field'],['density','Particle density',0.1,1,0.01,'Coverage of illuminated flow ridges'],['radius','Sphere radius',14,31,0.25,'Projected radius in LED pixels']],lfo:['Flow speed','Field scale','Particle density','Sphere radius']},
 {name:'2D Flow Field',params:[['b','Brightness',0,15,1,'Particle brightness through the selected palette'],['speed','Flow speed',0.05,8,0.05,'Planar particle speed; Flow energy controls this by default'],['scale','Field scale',0.3,4,0.05,'Spatial scale of the vector field'],['density','Particle density',0.05,1,0.01,'Number of particles moving through the field'],['curl','Curl strength',0.1,3,0.05,'How strongly paths bend and swirl'],['trails','Trail length',1,8,1,'Visible pixels behind each particle']],lfo:['Flow speed','Field scale','Particle density','Curl strength','Trail length']},
 {name:'Patternflow',params:[['b','Brightness',0,15,1,'Output brightness through the selected palette'],['c0','Control 1',0,100,1,'Pattern-specific control'],['c1','Control 2',0,100,1,'Pattern-specific control'],['c2','Control 3',0,100,1,'Pattern-specific control'],['c3','Control 4',0,100,1,'Pattern-specific control'],['zoom','Spatial zoom',0.5,2,0.01,'Scale the complete pattern around its center'],['rotation','Rotation',-180,180,1,'Rotate the complete pattern in degrees'],['symmetry','Symmetry',0,3,1,'0 none · 1 horizontal · 2 vertical · 3 four-way']],lfo:['Control 1','Control 2','Control 3','Control 4']}
];
const pfMeta=[["Vector Field Flow",["Turbulence","Velocity","Density","Palette"]],["Chromatic Vortex",["Split","Swirl speed","Density","Color bias"]],["Liquid Ripple",["Refraction","Speed","Waves","Specular"]],["PF Kaleidoscope",["Symmetry","Speed","Core depth","Twist"]],["Isometric Pillars",["Grid size","Speed","Height","Color shift"]],["Color Ribbons",["Hue","Speed","Ribbons","Amplitude"]],["Folded Cross",["Density","Speed","Folds","Color mask"]],["Bitwise Shards",["Resolution","Speed","Bit shift","Palette"]],["Blooming Bioluminescence",["Cells","Speed","Plasma","Base hue"]],["Chladni Acoustic Waves",["Mode N","Mode M","Speed","Edge sharpness"]],["Pulsating Solar Flare",["Density","Pulse speed","Wave frequency","Glow"]],["PsiOp Death Star",["Orbit speed","Point density","Dish size","Equator glow"]],["Pendulum Waves",["Animation speed","Base frequency","Frequency spread","Margin"]],["Nachtschwärmer",["Flow speed","Blob size","Gooeyness","Particles"]],["Cathedral Glass",["Drift speed","Pane size","Lead glow","Color spread"]],["Tidal Topography",["Tide speed","Contour spacing","Wave depth","Line weight"]],["Whitney Harmonics",["Canon speed","Voices","Orbit size","Persistence"]],["Velvet Steps",["Walk","Direction","Cat scale","Frost"]],["Vega444 C",["Grid","Bulge","Wave depth","Color drift"]],["Twisted Tunnel",["Ring spacing","Twist","Accent hue","Travel speed"]],["TV Setup",["Channel tuning","Interference","Snow","RGB convergence"]],["Soliton Lattice Echo",["Kink energy","Relative pace","Soliton count","Echo decay"]],["Ripple Cell Grid 2",["Wave speed","Cell size","Ripple depth","Dot width"]],["Poolside",["Pool depth","Wave drift","Chop","Swell wavelength"]],["Pareidolia",["Drift speed","Noise detail","Ink density","Domain warp"]],["Squircle Spacetime Warp",["Circle to square","Gravity lens","Grid frequency","Cell phase"]],["Mondrian",["Accent hue","Hold time","Cell count","Move speed"]],["Midnight Metropolis",["Scroll speed","City mood","Haze","Window activity"]],["Mercury Rain",["Rain rate","Ripple size","Expansion speed","Chrome contrast"]],["Hypno Circles",["Spectrum center","Motion speed","Wave ratio","White intersections"]],["Entropy Glass",["Neck width","Sorting demon","Gravity","Color blindness"]],["Collatz Kelp",["Odd turn angle","Growth speed","Even turn angle","Seed count"]],["Bus Window Rain",["Rain rate","Fog regrowth","Bus speed","Light focus"]],["Bioluminescent Topo Vortex",["Ring count","Rotation speed","Twist","Color bias"]],["Aizawa Viscous Ink Plume",["Viscosity bifurcation","Downflow","Vortex core","Plume thickness"]],["Whitney Harmonics II",["Canon speed","Voices","Orbit size","Trail length"]],["Chladni Resonance",["Mode N","Mode M","Morph speed","Filament width"]],["Vega444 C — Orbital",["Grid density","Lens bulge","Breathing depth","Orbit speed"]],["Warped Checkerboard — Silk",["Flow speed","Cell size","Warp strength","Fabric fold"]],["Bioluminescent Topo Vortex II",["Ring count","Flow speed","Vortex twist","Strand beads"]],["Ripple Cell Grid 2 — Liquid",["Wave speed","Cell size","Ripple depth","Bubble size"]],["Twisted Tunnel — Hyperspace",["Travel speed","Portal density","Twist","Flight path"]],["Squares — Infinite",["Travel speed","Square count","Corkscrew","Line weight"]],["Whitney — Motion Graphics 1968",["Motion speed","Line count","Phase spacing","Line width"]],["Whitney — Differential Columns",["Motion speed","Orbit radius","Phase divergence","Dot size"]],["Whitney — Chromatic Bloom",["Bloom speed","Particle count","Petal folding","Counterpoint"]],["Bioluminescent Topo Vortex — Abyssal",["Ring count","Flow speed","Vortex twist","Braiding"]],["Warped Checkerboard — Interference",["Motion speed","Wavelength","Distortion","Wave interference"]],["Ripples",["Expansion speed","Ring width","Spawn density","Decay"]],["Luminous Loom",["Weave speed","Ribbon count","Undulation","Ribbon width"]],["Ferrofluid",["Flow speed","Droplets","Cohesion","Metallic reflection"]],["Accretion",["Orbital speed","Disc tilt","Event horizon","Turbulence"]],["Origami Flight",["Motion speed","Facet count","Folding depth","Precession"]],["Signal Orchard",["Pulse speed","Trees","Branching depth","Surge"]],["Whitney — Resonant Orbits",["Canon speed","Particle count","Rate divergence","Echo length"]],["Whitney — Phase Architecture",["Motion speed","Strand count","Torsion","Precession"]],["Removed",["Unused","Unused","Unused","Unused"]],["Removed",["Unused","Unused","Unused","Unused"]],["Pong Wars",["Game speed","Tile size","Ball size","Bounce variation"]],["Whitney — Resonant Orbits v2",["Canon speed","Particle count","Rate divergence","Echo length"]],["Warped Checkerboard — Acid Cathedral",["Motion speed","Grid density","Arch distortion","Aliasing"]],["Warped Checkerboard — Phase Slippage",["Motion speed","Grid density","Phase warp","Aliasing"]],["Warped Checkerboard — Liquid Circuit",["Motion speed","Grid density","Liquid warp","Aliasing"]],["Ripples — Classic 08",["Drop frequency","Trail fade","Ring width","Wave speed"]],["Plasma — Classic 08",["Flow speed","Wave scale","Hue drift","Trail fade"]]];
const pfAuthors=["Patternflow Community","Patternflow Community","Patternflow Community","Patternflow Community","Patternflow Community","Patternflow Community","Patternflow Community","Patternflow Community","@WollyMeister","@engmung","@martinnlopez","aap · PsiOp","Imported Arduino sketch","Imported Arduino sketch","Imported Arduino sketch","Imported Arduino sketch","Imported Arduino sketch","@mikdnom · Patternflow","@WollyMeister · Patternflow","@kpow · Patternflow","@petrusha · Patternflow","@engmung · Patternflow","@engmung · Patternflow","@engmung · Patternflow","@martinnlopez · Patternflow","@martinnlopez · Patternflow","@kpow · Patternflow","@martinnlopez · Patternflow","@Besoiobiy · Patternflow","@SimonePDA · Patternflow","@engmung · Patternflow","@engmung · Patternflow","@engmung · Patternflow","@martinnlopez · Patternflow","@engmung · Patternflow","Imported Arduino sketch · remix","@engmung · remix","@WollyMeister · remix","Built-in C++ · remix","@martinnlopez · remix","@engmung · remix","@kpow · remix","Built-in C++ · remix","John Whitney · Kenny Kim recreation","John Whitney study · user-supplied sketch","John Whitney study · user-supplied sketch","@martinnlopez · advanced variation","Built-in C++ · advanced variation","User-supplied Arduino algorithm","Original C++ · Nova studies","Original C++ · Nova studies","Original C++ · Nova studies","Original C++ · Nova studies","Original C++ · Nova studies","Original C++ · John Whitney inspiration","Original C++ · John Whitney inspiration","","","Koen van Gilst · MIT · native adaptation","Original C++ · John Whitney inspiration","Original C++ · Acid studies","Original C++ · Acid studies","Original C++ · Acid studies","User sketch · LED_Matrix_08","User sketch · LED_Matrix_08"];
const corePatternIds=[0,1,3,5,6,7,9,11,12,14];
const collectionPatternIds=[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,18,19,21,22,23,25,29,31,33,35,36,38,42,43,45,46,47,48,49,50,51,52,53,54,55,58,59,60,61,62,63,64];
const galleryItems=[...corePatternIds.map(i=>({key:'n'+i,name:defs[i].name,engine:i,variant:null,source:'Built-in C++',seed:i})),...collectionPatternIds.map(i=>({key:'p'+i,name:pfMeta[i][0],engine:16,variant:i,source:pfAuthors[i],seed:16+i}))];
galleryItems.forEach((item,index)=>item.previewIndex=index);
const previewAtlas=new Image();let previewAtlasReady=false,previewAtlasKey=null;previewAtlas.onload=()=>previewAtlasReady=true;
const paletteNames=['Spectrum','Analogous','Split complement','Sunset','Ocean','Neon','Custom','Original','Ember','Ice','Reef','Soukan 1','Soukan 2','Soukan 3','Soukan 4','Soukan 5','Soukan 6','Soukan 7','Soukan 8','Soukan 9','Soukan 10'];
const styleDefs=[['zoom','Spatial zoom',0.5,2,0.01,'Scale the complete pattern around its center'],['rotation','Rotation',-180,180,1,'Rotate the complete pattern in degrees'],['offsetX','Horizontal offset',-32,32,1,'Move the rendered pattern left or right'],['offsetY','Vertical offset',-32,32,1,'Move the rendered pattern up or down'],['symmetry','Symmetry',0,3,1,'0 none · 1 horizontal · 2 vertical · 3 four-way']];
let state=null,timers={},lastStateRevision=null,onlyBookmarks=false;
const $=id=>document.getElementById(id);
function setTheme(theme,persist=true){if(!['field','graphite','signal'].includes(theme))theme='field';document.documentElement.dataset.theme=theme;document.querySelectorAll('[data-theme-choice]').forEach(button=>button.setAttribute('aria-pressed',button.dataset.themeChoice===theme));document.querySelector('meta[name="theme-color"]').content={field:'#e4e3db',graphite:'#1b201f',signal:'#dfbf32'}[theme];if(persist)try{localStorage.setItem('matrix-studio-theme',theme)}catch(e){}}
function setLibraryFilter(starred){onlyBookmarks=starred;$('allFilter').setAttribute('aria-pressed',!starred);$('starFilter').setAttribute('aria-pressed',starred);renderPatternGallery()}
function updateActivePattern(){const item=galleryItems.find(item=>item.key===selectedGalleryKey());if(!item)return;const archived=metadataFor(item).archived;$('activePatternName').textContent=archived?'Library empty':item.name;$('activePatternSource').textContent=archived?'Open Archive / restore to resume':String(item.previewIndex+1).padStart(2,'0')+' / '+galleryItems.length+' · '+item.source;const canvas=$('activePreview');canvas.style.visibility=archived?'hidden':'visible';canvas.dataset.seed=item.seed;canvas.dataset.preview=item.previewIndex;canvas.setAttribute('aria-label',item.name+' animation preview')}
function toast(message,ok=true){const t=$('toast');t.textContent=message;t.className='toast show '+(ok?'ok':'bad');clearTimeout(toast._t);toast._t=setTimeout(()=>t.className='toast',2600)}
async function api(url,opt){const r=await fetch(url,opt);let d={};try{d=await r.json()}catch(e){}if(!r.ok||d.success===false)throw new Error(d.message||('HTTP '+r.status));return d}
async function post(body){return api('/control',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})}
async function command(command,extra={}){try{const d=await post({command,...extra});toast(d.message||'Updated');if(['setPattern','setPatternVariant','loadPatch','setDefault','setBookmark','setPatternQuality'].includes(command))await refreshState()}catch(e){toast(e.message,false)}}
function selectedGalleryKey(){return state&&state.currentPattern===16?'p'+Number(state.params.variant||0):'n'+Number(state?state.currentPattern:0)}
async function selectGalleryPattern(item){if(item.variant===null)await command('setPattern',{value:item.engine});else await command('setPatternVariant',{value:item.variant})}
// LIBRARY_ORGANIZER_START — pure model helpers are tested from this source.
let libraryData={folders:[],families:[],tags:[],entries:[],readable:true},libraryLoaded=false;
const libraryDrafts=new Map();
function metadataId(item){return item.variant===null?item.engine:16+item.variant}
function defaultFamily(item){
 const sets=[['Warped Checkerboard','n1','p38','p47','p60','p61','p62'],['Whitney Harmonics','p16','p35'],['Chladni Acoustic Waves','p9','p36'],['Vega444 C','p18','p37'],['Bioluminescent Topo Vortex','p33','p39','p46'],['Ripple Cell Grid 2','p22','p40'],['Twisted Tunnel','p19','p41'],['Squares','n12','p42'],['Whitney — Resonant Orbits','p54','p59']];
 return sets.find(g=>g.slice(1).includes(item.key))?.[0]||item.name;
}
function metadataFor(item){const row=libraryData.entries.find(e=>e[0]===metadataId(item))||[0,0,0,0,0];return {rating:row[1],folder:libraryData.folders[row[2]-1]||'',family:libraryData.families[row[3]-1]||'',tags:libraryData.tags.filter((s,i)=>s&&(row[4]&(1<<i))),archived:!!row[5]}}
function familyFor(item){return metadataFor(item).family||defaultFamily(item)}
function librarySelection(items,options){
 const query=options.query.trim().toLowerCase(),marked=x=>!!state.bookmarks?.[x.previewIndex];
 const filtered=items.filter(x=>{const m=metadataFor(x);return m.archived===(options.view==='archived')&&(!options.starred||marked(x))&&(!query||[x.name,x.source,m.folder,familyFor(x),...m.tags].join(' ').toLowerCase().includes(query))&&(!options.folder||(options.folder==='__unfiled__'?!m.folder:m.folder===options.folder))&&(!options.tag||m.tags.includes(options.tag))&&(options.rating===-1?!m.rating:m.rating>=options.rating)});
 const rank=(a,b)=>Number(marked(b))-Number(marked(a))||(options.sort==='rating'?metadataFor(b).rating-metadataFor(a).rating:0)||(options.sort==='name'?a.name.localeCompare(b.name):a.previewIndex-b.previewIndex);
 if(!options.group)return filtered.sort(rank);
 const groups=new Map();for(const item of filtered){const family=familyFor(item);if(!groups.has(family))groups.set(family,[]);groups.get(family).push(item)}
 return [...groups.values()].map(g=>g.sort(rank)).sort((a,b)=>rank(a[0],b[0])).flat();
}
// LIBRARY_ORGANIZER_MODEL_END
function libraryOptions(id,values,first){const root=$(id),previous=root.value;root.replaceChildren();for(const [value,label] of first.concat([...new Set(values.filter(Boolean))].sort().map(x=>[x,x]))){const o=document.createElement('option');o.value=value;o.textContent=label;root.appendChild(o)}root.value=previous;if(root.selectedIndex<0)root.selectedIndex=0}
function renderLibraryFilters(){libraryOptions('folderFilter',libraryData.folders,[['','All folders'],['__unfiled__','Unfiled']]);libraryOptions('tagFilter',libraryData.tags,[['','All tags']]);for(const [id,names] of [['folderNames',libraryData.folders],['familyNames',galleryItems.map(familyFor)]]){const root=$(id);root.replaceChildren();for(const name of [...new Set(names.filter(Boolean))].sort()){const o=document.createElement('option');o.value=name;root.appendChild(o)}}}
function activeLibraryItem(){return galleryItems.find(x=>x.key===selectedGalleryKey())}
function currentLibraryDraft(){const item=activeLibraryItem();if(!item)return null;return libraryDrafts.get(item.key)||{...metadataFor(item),tags:metadataFor(item).tags.join(', ')}}
function editLibraryDraft(field,value){const item=activeLibraryItem();if(!item)return;libraryDrafts.set(item.key,{...currentLibraryDraft(),[field]:value});renderRating();$('organizationStatus').textContent='Unsaved changes'}
function renderRating(){const draft=currentLibraryDraft();if(!draft)return;const root=$('patternRating');root.replaceChildren();for(let i=1;i<=5;i++){const b=document.createElement('button');b.type='button';b.textContent=i<=draft.rating?'★':'☆';b.setAttribute('aria-label',i+' of 5 stars');b.setAttribute('aria-pressed',i<=draft.rating);b.onclick=()=>editLibraryDraft('rating',i);root.appendChild(b)}const clear=document.createElement('button');clear.className='clearRating';clear.textContent='Clear';clear.type='button';clear.onclick=()=>editLibraryDraft('rating',0);root.appendChild(clear)}
function renderLibraryEditor(){const item=activeLibraryItem(),draft=currentLibraryDraft();if(!draft)return;const root=$('libraryEditor'),changed=root.dataset.key!==item.key;root.dataset.key=item.key;renderRating();for(const [id,key] of [['metaFolder','folder'],['metaFamily','family'],['metaTags','tags']])if(changed||document.activeElement!==$(id))$(id).value=draft[key];$('metaFamily').placeholder=defaultFamily(item);$('organizationStatus').textContent=!libraryData.readable?'Storage unreadable — not overwritten':libraryDrafts.has(item.key)?'Unsaved changes':'Saved on matrix'}
function resetOrganizationDraft(){const item=activeLibraryItem();if(item)libraryDrafts.delete(item.key);renderLibraryEditor()}
async function saveOrganization(){const item=activeLibraryItem(),draft=currentLibraryDraft();if(!item||!draft)return;const value={id:metadataId(item),rating:draft.rating,folder:draft.folder.trim(),family:draft.family.trim(),tags:[...new Set(draft.tags.split(',').map(x=>x.trim().toLowerCase()).filter(Boolean))]};try{const result=await post({command:'setLibraryMetadata',value});libraryDrafts.delete(item.key);libraryLoaded=false;await refreshState();toast(result.message)}catch(e){toast(e.message,false)}}
function renderPatternGallery(){
 const root=$('patternGallery'),group=$('groupFamilies').checked;
 const items=librarySelection(galleryItems,{query:$('patternSearch').value||'',starred:onlyBookmarks,folder:$('folderFilter').value,tag:$('tagFilter').value,rating:Number($('ratingFilter').value),sort:$('librarySort').value,group,view:$('archiveView').value});
 const signature=JSON.stringify([group,items.map(x=>[x.key,!!state.bookmarks?.[x.previewIndex],metadataFor(x),familyFor(x)])]);
 $('libraryCount').textContent=items.length+' / '+galleryItems.length;
 if(root.dataset.signature!==signature){root.dataset.signature=signature;root.replaceChildren();let family=null;
  const familyCounts=new Map();for(const item of items)familyCounts.set(familyFor(item),(familyCounts.get(familyFor(item))||0)+1);
  for(const item of items){if(group&&family!==familyFor(item)){if(familyCounts.get(family)>1){const end=document.createElement('div');end.style.gridColumn='1/-1';root.appendChild(end)}family=familyFor(item);if(familyCounts.get(family)>1){const heading=document.createElement('div');heading.className='familyHeading';heading.textContent=family;root.appendChild(heading)}}
   const card=document.createElement('div');card.className='patternCard';card.dataset.key=item.key;const marked=!!state.bookmarks?.[item.previewIndex];
   // Only compiled names go into markup; all editable metadata uses textContent.
   card.innerHTML=`<button class="patternSelect" type="button" aria-label="Play ${item.name}"><canvas width="28" height="28" data-seed="${item.seed}" data-preview="${item.previewIndex}"></canvas><div class="patternInfo"><strong>${item.name}</strong><span>${item.source}</span></div></button><span class="patternRating"></span><button class="bookmark ${marked?'marked':''}" type="button" aria-label="${marked?'Remove bookmark':'Bookmark'} ${item.name}" aria-pressed="${marked}">${marked?'⚑':'⚐'}</button>`;
   const m=metadataFor(item);card.querySelector('.patternRating').textContent=m.rating?'★'.repeat(m.rating)+'☆'.repeat(5-m.rating):'— not rated';card.title=[item.name,m.folder,...m.tags].filter(Boolean).join(' · ');
   const familyPrefix=familyFor(item)+' — ';if(item.name.startsWith(familyPrefix))card.querySelector('.patternInfo strong').textContent=item.name.slice(familyPrefix.length);
   const archive=document.createElement('button');archive.type='button';archive.className='filterButton archiveAction';archive.style.cssText='width:100%;border-top:1px solid var(--line);padding:7px';archive.textContent=m.archived?'↩ Restore':'↓ Archive';archive.setAttribute('aria-label',(m.archived?'Restore ':'Archive ')+item.name);archive.onclick=()=>setPatternArchived(item,!m.archived);card.appendChild(archive);
   const play=card.querySelector('.patternSelect');play.disabled=m.archived;play.setAttribute('aria-label',(m.archived?'Archived: ':'Play ')+item.name);
   play.onclick=()=>selectGalleryPattern(item);card.querySelector('.bookmark').onclick=()=>command('setBookmark',{value:{index:item.previewIndex,enabled:!marked}});root.appendChild(card);
  }
  if(!items.length){const empty=document.createElement('div');empty.className='emptyLibrary';empty.textContent=$('archiveView').value==='archived'?'No matching archived patterns.':'No matching patterns. Clear filters or restore a pattern from Archive.';root.appendChild(empty)}
 }
 const selected=selectedGalleryKey();root.querySelectorAll('.patternCard').forEach(card=>card.classList.toggle('selected',card.dataset.key===selected));renderLibraryEditor();
}
async function setPatternArchived(item,archived){
 try{const result=await post({command:'setPatternArchived',value:{id:metadataId(item),archived}});libraryLoaded=false;await refreshState();toast(result.message)}catch(e){toast(e.message,false)}
}
function hsvRgb(h,s,v){h=((h%1)+1)%1;const i=Math.floor(h*6),f=h*6-i,p=v*(1-s),q=v*(1-f*s),r=v*(1-(1-f)*s);const a=[[v,r,p],[q,v,p],[p,v,r],[p,q,v],[r,p,v],[v,p,q]][i%6];return a.map(x=>Math.max(0,Math.min(255,Math.round(x*255))))}
function previewSample(seed,x,y,t){
 const dx=x-.5,dy=y-.5,r=Math.hypot(dx,dy),a=Math.atan2(dy,dx),mode=seed%10;let v=0,h=(seed*.083+t*.015)%1;
 if(seed===36){const centers=[[.22*Math.sin(t*.7),.18*Math.cos(t*.9)],[.27*Math.sin(t*.53+2),.24*Math.cos(t*.67+1)],[.3*Math.sin(t*.41+4),.2*Math.cos(t*.59+3)]];const d=centers.map(c=>Math.hypot(dx-c[0],dy-c[1])).sort((u,w)=>u-w);v=Math.min(1,Math.exp(-d[0]*9)+Math.exp(-(d[1]-d[0])*30)*.55);h+=d[0]*.6;}
 else if(seed===37){const n=3+Math.sin(t)*.5,m=5+Math.cos(t*.8)*.5,ch=Math.cos(n*Math.PI*dx*2)*Math.cos(m*Math.PI*dy*2)-Math.cos(m*Math.PI*dx*2)*Math.cos(n*Math.PI*dy*2);v=Math.exp(-Math.abs(ch)*10);h=.76+dx*.12+dy*.12;}
 else if(seed===38){const row=Math.floor((y+t*.15)*28),col=Math.floor(x*28),noise=((Math.sin(col*91.7+row*43.1)*43758.5)%1+1)%1;v=noise>.91?1:0;if(v){const trail=((Math.sin(col*17.3+row*11.9)*9)|0)&3;v=Math.max(.18,1-trail*.22)}h=.56+x*.1;}
 else if(seed===39){const spiral=Math.abs(Math.sin(a*12-r*105+t*2.2)),pulse=.5+.5*Math.sin(r*46-t*3);v=Math.pow(Math.max(0,1-spiral*4),1.4)*(.45+.55*pulse)*Math.max(0,1-r*.9);h=.98+r*.18;}
 else if(seed===40){const band=Math.abs(dy-.15*Math.sin(dx*8+t*.8)-.06*Math.sin(dx*19-t)),gx=Math.floor(x*34),gy=Math.floor(y*34),noise=((Math.sin(gx*83.1+gy*27.7+Math.floor(t*8))*43758.5)%1+1)%1;v=band<.17&&noise>.78?1:0;h=.58+dx*.2;}
 else if(seed===41){const ca=Math.cos(t*.35),sa=Math.sin(t*.35),qx=dx*ca-dy*sa,qy=dx*sa+dy*ca,warp=.055*Math.sin(a*3-t*.7),ring=Math.abs(Math.hypot(qx*1.28,qy)-(.235+warp)),inner=Math.abs(Math.hypot(qx*.78,qy)-(.125-.025*Math.sin(a*2+t)));v=Math.min(1,Math.exp(-ring*34)+Math.exp(-inner*48)*.7)*Math.max(0,1-r*.72);h=.72+a/(Math.PI*2)+t*.025;}
 else if(seed===42){const ca=Math.cos(t*.7),sa=Math.sin(t*.7),u=dx*ca-dy*sa,w=dx*sa+dy*ca;v=(Math.abs(Math.abs(u)-.25)<.025||Math.abs(Math.abs(w)-.25)<.025)?1:0;h=.55+a/(Math.PI*2);}
 else if(seed===43){const dish=Math.hypot(dx-.16*Math.cos(t*.45),dy-.16*Math.sin(t*.45));v=r<.34&&(((Math.floor(x*30)+Math.floor(y*30)*7)%5===0)||Math.abs(r-.31)<.014)?1:0;if(dish<.1)v=0;h=.58+r*.2;}
 else if(seed===44){const ca=Math.cos(t*.55),sa=Math.sin(t*.55),u=dx*ca-dy*sa,w=dx*sa+dy*ca;v=Math.max(0,1-Math.max(Math.abs(u),Math.abs(w))*2.8);h=.08+a/(Math.PI*2)+t*.02;}
 else if(mode===0)v=.5+.5*Math.sin(r*46-t*3);else if(mode===1)v=((Math.floor((x+.08*Math.sin(y*9+t))*10)+Math.floor((y+.08*Math.cos(x*8-t))*10))&1);else if(mode===2)v=.5+.5*Math.sin((dx*Math.cos(t)+dy*Math.sin(t))*42+Math.sin(r*22-t)*3);else if(mode===3)v=Math.max(0,1-Math.abs(Math.sin((a*5+r*18-t*2))));else if(mode===4)v=Math.pow(.5+.5*Math.sin((x+y)*18+Math.sin(x*15-t)*4+t),2);else if(mode===5)v=Math.max(0,.16-Math.abs(r-(.12+((t*.08+seed*.07)%1)*.65)))*7;else if(mode===6)v=((Math.floor(x*12)^Math.floor(y*12)^Math.floor(t*2))&3)===0?1:.08;else if(mode===7)v=Math.max(0,1-Math.abs(dy-Math.sin(x*15+t+seed)*.18)*14);else if(mode===8)v=Math.pow(Math.abs(Math.sin(r*30-a*3-t*2)),3);else v=Math.random()<(0.015+.025*(.5+.5*Math.sin(t+seed)))?1:0;
 h+=a/(Math.PI*2)+r*.35;return hsvRgb(h,.78,Math.max(.025,Math.min(1,v)))}
let selectedControlId=null,controlRegistry={};
document.addEventListener("keydown",e=>{if(e.defaultPrevented||e.ctrlKey||e.metaKey||e.altKey||!["ArrowUp","ArrowDown","ArrowLeft","ArrowRight"].includes(e.key))return;const control=controlRegistry[selectedControlId];if(!control||!control.box.isConnected)return;const el=e.target,tag=el?.tagName;if(tag==="TEXTAREA"||tag==="SELECT"||el?.isContentEditable)return;if(tag==="INPUT"&&!control.box.contains(el))return;e.preventDefault();control.adjust(["ArrowUp","ArrowRight"].includes(e.key)?1:-1,e.shiftKey)});
let previewLast=0;function animatePatternPreviews(now){if(now-previewLast>=(state?.previewIntervalMs||67)){previewLast=now-((now-previewLast)%(state?.previewIntervalMs||67));const frame=Math.floor(now/(state?.previewIntervalMs||67))%(state?.previewFrames||24),pw=state?.previewWidth||28,ph=state?.previewHeight||28;document.querySelectorAll('.patternCard canvas,#activePreview').forEach(canvas=>{const ctx=canvas.getContext('2d'),w=canvas.width,h=canvas.height;if(previewAtlasReady){ctx.imageSmoothingEnabled=false;ctx.drawImage(previewAtlas,frame*pw,Number(canvas.dataset.preview)*ph,pw,ph,0,0,w,h);return}ctx.fillStyle='#111';ctx.fillRect(0,0,w,h)})}requestAnimationFrame(animatePatternPreviews)}
function debounce(key,fn){clearTimeout(timers[key]);timers[key]=setTimeout(fn,130)}
function makeSlider(parent,p,current,anim){const [key,label,min,max,step,help]=p;const id=anim+':'+(anim===16?state?.params.variant:'native')+':'+key;const box=document.createElement('div');box.className='control';box.dataset.controlId=id;box.title=help;const number=parent.id==='paramControls'?'<span class="controlNumber">'+String(parent.children.length+1).padStart(2,'0')+'</span>':'';box.innerHTML='<div class="controlTop"><label for="r_'+key+'">'+number+label+'</label><span class="value" id="v_'+key+'">'+current+'</span></div><div class="encoderRow"><div class="encoder" role="slider" tabindex="0" aria-label="'+label+'" aria-valuemin="'+min+'" aria-valuemax="'+max+'" aria-valuenow="'+current+'"><span class="needle"></span></div></div><div class="rangeRow"><input aria-label="'+label+'" id="r_'+key+'" type="range" min="'+min+'" max="'+max+'" step="'+step+'" value="'+current+'"><input aria-label="'+label+' value" id="n_'+key+'" class="miniInput" type="number" min="'+min+'" max="'+max+'" step="'+step+'" value="'+current+'"></div>';parent.appendChild(box);const range=box.querySelector('#r_'+key),num=box.querySelector('#n_'+key),val=box.querySelector('#v_'+key),dial=box.querySelector('.encoder');const select=()=>{selectedControlId=id;document.querySelectorAll('[data-control-id]').forEach(el=>el.classList.toggle('keyboardSelected',el.dataset.controlId===id))};const update=(raw,send=true)=>{const parsed=Number(raw);if(!Number.isFinite(parsed))return;let v=Math.max(min,Math.min(max,parsed));v=Number((min+Math.round((v-min)/step)*step).toFixed(4));v=Math.max(min,Math.min(max,v));range.value=v;num.value=v;val.textContent=v;dial.style.setProperty('--angle',(-135+270*(v-min)/(max-min))+'deg');dial.setAttribute('aria-valuenow',v);if(send){select();if(state&&state.currentPattern===anim)state.params[key]=v;debounce(anim+'_'+key,()=>command('setParam',{animIndex:anim,paramName:key,value:v}))}};controlRegistry[id]={box,adjust(direction,coarse){update(Number(range.value)+direction*step*(coarse?10:1),true)}};update(current,false);box.classList.toggle('keyboardSelected',selectedControlId===id);range.oninput=()=>{select();update(range.value,false)};range.onchange=()=>update(range.value,true);num.onchange=()=>update(num.value,true);let dragging=false,startY=0,startValue=0;dial.onpointerdown=e=>{if(e.button!==0)return;e.preventDefault();select();dragging=true;startY=e.clientY;startValue=Number(range.value);dial.setPointerCapture(e.pointerId);dial.focus()};dial.onpointermove=e=>{if(dragging)update(startValue+(startY-e.clientY)*(max-min)/160*(e.shiftKey?.1:1),false)};dial.onpointerup=e=>{if(!dragging)return;dragging=false;update(range.value,true);if(dial.hasPointerCapture(e.pointerId))dial.releasePointerCapture(e.pointerId)};dial.onpointercancel=()=>{dragging=false;update(startValue,false)};let keyboardEdit=false;dial.onkeydown=e=>{if(!['ArrowUp','ArrowDown','ArrowLeft','ArrowRight','Home','End','PageUp','PageDown'].includes(e.key))return;e.preventDefault();select();keyboardEdit=true;const delta=step*(e.shiftKey?10:1);let v=Number(range.value);if(e.key==='Home')v=min;else if(e.key==='End')v=max;else v+=(['ArrowUp','ArrowRight','PageUp'].includes(e.key)?1:-1)*delta*(e.key.startsWith('Page')?10:1);update(v,false)};dial.onkeyup=()=>{if(keyboardEdit){keyboardEdit=false;update(range.value,true)}};dial.onblur=()=>{if(keyboardEdit){keyboardEdit=false;update(range.value,true)}}}
function renderColorControls(){const root=$('colorControls');root.innerHTML='';const palette=document.createElement('div');palette.className='control';palette.innerHTML='<div class="controlTop"><label>Palette</label></div><select id="palette">'+paletteNames.map((name,i)=>`<option value="${i}">${name}</option>`).join('')+'</select><p class="help">Every palette owns four editable gradient stops</p>';root.appendChild(palette);const select=palette.querySelector('select');select.value=state.params.palette;select.onchange=async e=>{await command('setParam',{animIndex:state.currentPattern,paramName:'palette',value:Number(e.target.value)});await refreshState()};const gradient=document.createElement('div');gradient.className='control';const colors=[0,1,2,3].map(i=>state.params['color'+i]);gradient.innerHTML='<div class="controlTop"><label>'+paletteNames[state.params.palette]+' gradient</label></div><div class="gradientPreview"></div><div class="colorStops">'+colors.map((color,i)=>`<input type="color" value="${color}" aria-label="Gradient color ${i+1}">`).join('')+'</div><p class="help">Four saved stops; select another palette to edit its own colors</p>';root.appendChild(gradient);const preview=gradient.querySelector('.gradientPreview'),paint=()=>preview.style.background=`linear-gradient(90deg,${colors.join(',')},${colors[0]})`;paint();gradient.querySelectorAll('input').forEach((input,i)=>input.onchange=()=>{colors[i]=input.value;state.params['color'+i]=input.value;paint();command('setParam',{animIndex:state.currentPattern,paramName:'color'+i,value:input.value})});makeSlider(root,['hue','Palette phase',0,359,1,'Move through the gradient without changing its colors'],state.params.hue,state.currentPattern);makeSlider(root,['spread','Palette span',0,360,1,'Compress or expand the traversed gradient range'],state.params.spread,state.currentPattern);makeSlider(root,['globalHueShift','Global hue shift',-180,180,1,'Rotate every gradient color around the hue wheel'],state.params.globalHueShift,state.currentPattern);makeSlider(root,['globalSaturation','Global saturation',0,200,1,'0% grayscale · 100% original · 200% boosted'],state.params.globalSaturation,state.currentPattern)}
function renderParams(){controlRegistry={};const d=defs[state.currentPattern],params=$('paramControls');if(state.currentPattern===16){const meta=pfMeta[Math.max(0,Math.min(pfMeta.length-1,Number(state.params.variant)||0))];for(let i=0;i<4;i++)d.params[i+1][1]=meta[1][i];d.lfo=meta[1]}params.innerHTML='';const visible=d.params.concat(styleDefs.slice(0,Math.max(0,8-d.params.length)));visible.slice(0,8).forEach(p=>makeSlider(params,p,state.params[p[0]],state.currentPattern));$('colorBox').style.display='';renderColorControls();const lfo=$('lfoControls');lfo.innerHTML='';const target=document.createElement('div');target.className='control';target.innerHTML='<div class="controlTop"><label>Target</label></div><select id="lfoTarget"><option value="0">Off</option>'+d.lfo.map((x,i)=>`<option value="${i+1}">${x}</option>`).join('')+'</select><p class="help">Parameter controlled by the sine LFO</p>';lfo.appendChild(target);target.querySelector('select').value=state.params.lfo_target;target.querySelector('select').onchange=e=>command('setParam',{animIndex:state.currentPattern,paramName:'lfo_target',value:Number(e.target.value)});makeSlider(lfo,['lfo_speed','LFO speed',0.05,10,0.05,'Oscillation speed'],state.params.lfo_speed,state.currentPattern);makeSlider(lfo,['lfo_amount','LFO amount',0,1,0.01,'Modulation depth from 0 to 100%'],state.params.lfo_amount,state.currentPattern)}
function renderPatches(){const root=$('patches');root.innerHTML='';for(let i=0;i<3;i++){const p=document.createElement('div');p.className='patch '+(state.defaultPatches[state.currentPattern]===i?'isDefault':'');p.innerHTML=`<strong>Patch ${i+1}<span class="defaultMark" title="Auto-cycle default" aria-label="Auto-cycle default"></span></strong><div class="buttonRow"><button class="btn ghost">Load</button><button class="btn primary">Save</button><button class="btn ghost" title="Use patch for auto-cycle">Auto</button></div>`;const b=p.querySelectorAll('button');b[0].onclick=()=>command('loadPatch',{animIndex:state.currentPattern,patchIndex:i});b[1].onclick=()=>{if(confirm('Overwrite patch '+(i+1)+' with the current settings?'))command('savePatch',{animIndex:state.currentPattern,patchIndex:i})};b[2].onclick=()=>command('setDefault',{animIndex:state.currentPattern,patchIndex:i});root.appendChild(p)}}
function minutesToTime(value){const m=Math.max(0,Math.min(1439,Number(value)||0));return String(Math.floor(m/60)).padStart(2,'0')+':'+String(m%60).padStart(2,'0')}function timeToMinutes(value){const p=String(value||'00:00').split(':').map(Number);return Math.max(0,Math.min(1439,p[0]*60+p[1]))}
function render(){renderCalibration();$('qualityControl').style.display=state.currentPattern===16?'':'none';$('patternQuality').value=state.patternQuality||0;renderPatternGallery();updateActivePattern();$('autoCycle').checked=state.autoCycleEnabled;$('cycleInterval').value=state.cycleInterval;$('fps').value=state.targetFPS;$('resyncInterval').value=state.resyncInterval;$('scheduleEnabled').checked=state.scheduleEnabled;$('scheduleStart').value=minutesToTime(state.scheduleStartMinutes);$('scheduleEnd').value=minutesToTime(state.scheduleEndMinutes);$('scheduleFadeIn').value=state.scheduleFadeInSeconds;$('scheduleFadeOut').value=state.scheduleFadeOutSeconds;$('deviceTitle').textContent=state.deviceName||'Matrix.hub';$('deviceSubtitle').textContent=`Matrix.hub by Televator / ${state.displayWidth}×${state.displayHeight} / ${state.backend}`;document.title=`${state.deviceName||'Matrix'} — Matrix.hub by Televator`;$('resyncHelp').textContent=state.backend==='ws2812'?'not needed for WS2812':'leave at 0 for HUB75';$('modeBadge').textContent=state.networkMode==='ap'?'Setup access point':'Wi-Fi connected';$('ipBadge').textContent=state.ip;const net=state.networkMode==='ap'?`<strong>Setup mode</strong><br>Connect to <strong>${state.apSsid}</strong><br>Open <strong>${state.ip}</strong>`:`<strong>${state.ssid||'Wi-Fi'}</strong><br>IP: <strong>${state.ip}</strong><br>Signal: <strong>${state.rssi} dBm</strong><br>Address: <strong>http://${state.hostname}.local</strong>`;$('networkInfo').innerHTML=net;const luaButton=$('luaHelloButton');luaButton.disabled=!state.luaHelloAvailable;luaButton.textContent=state.luaHelloEnabled?'Stop Hello Lua':'Run Hello Lua';luaButton.title=state.luaHelloAvailable?'Lua calls the new C++ graphics backend':(state.luaError||'Lua runtime unavailable');const depth=$('depthTestButton');depth.style.display=state.depthComparisonAvailable?'':'none';depth.textContent=state.depthComparisonEnabled?'Stop 3-bit / 4-bit comparison':'Run 3-bit / 4-bit comparison';renderParams();renderPatches()}
async function refreshState(){try{state=await api('/control');if(!libraryLoaded||libraryData.revision!==state.libraryRevision){libraryData=await api('/library');libraryLoaded=true;renderLibraryFilters()}$('catalogCount').textContent=(state.catalog?.length||59)+' patterns';const key=state.previewAtlasKey||'acid-archive-59-24';if(key!==previewAtlasKey){previewAtlasKey=key;previewAtlasReady=false;previewAtlas.src='/preview-atlas.png?v='+key}render();renderSavedNetworks()}catch(e){toast(e.message,false)}}
async function refreshDiagnostics(){try{const d=await api('/diagnostics');$('liveFPS').textContent=d.actualFPS.toFixed(1);$('liveRefresh').textContent=d.panelRefreshHz.toFixed(0);$('liveResolution').textContent=(d.renderWidth||state?.displayWidth||64)+'×'+(d.renderHeight||state?.displayHeight||64)+' render';$('diagPattern').textContent=d.pattern;$('diagEngine').textContent=d.engine.toUpperCase()+' · '+d.cpuMHz+' MHz · '+d.colorDepth+'-bit';$('diagFPS').textContent=d.actualFPS.toFixed(1)+' / '+d.targetFPS+' FPS';$('diagRefreshLabel').textContent=d.refreshLabel||'Display refresh';$('diagRefresh').textContent=d.panelRefreshHz.toFixed(1)+' Hz';$('diagLoad').textContent=d.renderBudgetPercent.toFixed(0)+'%';$('diagAverage').textContent=(d.averageRenderUs/1000).toFixed(2)+' ms';$('diagWorst').textContent=(d.worstRenderUs/1000).toFixed(2)+' ms';$('diagDraw').textContent=(d.averageDrawUs/1000).toFixed(2)+' ms';$('diagShow').textContent=(d.averageShowUs/1000).toFixed(2)+' ms';$('diagOverBudget').textContent=d.overBudgetFrames+' / '+d.sampleFrames;$('diagHeap').textContent=(d.freeHeap/1024).toFixed(0)+' / '+(d.minimumFreeHeap/1024).toFixed(0)+' KB';const load=$('diagLoad');load.className=d.renderBudgetPercent>100?'diagBad':d.renderBudgetPercent>75?'diagWarn':'diagGood';$('diagStatus').textContent=d.actualFPS+0.5<d.targetFPS?'Render work cannot currently sustain the target frame rate.':'Render work is within the selected frame budget.';if(lastStateRevision===null)lastStateRevision=d.stateRevision;else if(lastStateRevision!==d.stateRevision){lastStateRevision=d.stateRevision;await refreshState()}}catch(e){$('diagStatus').textContent='Diagnostics temporarily unavailable: '+e.message}}
async function savePatternDefault(){if(!confirm('Permanently overwrite this pattern’s auto-cycle default with the current parameters?'))return;await command('saveCurrentDefault');await refreshState()}
async function savePlayback(){try{await command('setAutoCycle',{value:$('autoCycle').checked});await command('setCycleInterval',{value:Number($('cycleInterval').value)});await command('setFPS',{value:Number($('fps').value)});await command('setSchedule',{enabled:$('scheduleEnabled').checked,startMinutes:timeToMinutes($('scheduleStart').value),endMinutes:timeToMinutes($('scheduleEnd').value),fadeInSeconds:Number($('scheduleFadeIn').value),fadeOutSeconds:Number($('scheduleFadeOut').value)});await command('setResyncInterval',{value:Number($('resyncInterval').value)});await refreshState()}catch(e){toast(e.message,false)}}
async function toggleLuaHello(){await command(state.luaHelloEnabled?'stopLuaHello':'runLuaHello');await refreshState()}
function renderCalibration(){const c=state.calibration;if(!c)return;$('calibrationPanel').style.display=c.available?'':'none';$('calibrationStatus').textContent=c.bits+'-bit driver · RGB '+c.r+' / '+c.g+' / '+c.b+'% · '+(c.mode?'TEST ACTIVE':'playback');const fields={calMode:c.mode,calPhoto:c.photo,calLevel:c.level,calR:c.r,calG:c.g,calB:c.b,calGamma:c.gamma100/100};for(const [id,value] of Object.entries(fields)){const el=$(id);if(document.activeElement!==el)el.value=value;}$('calCorrect').checked=c.photoCorrection;$('calDepthOption').disabled=!state.depthComparisonAvailable;}
async function applyColorCalibration(){await command('setColorCalibration',{value:{r:Number($('calR').value),g:Number($('calG').value),b:Number($('calB').value),gamma100:Math.round(Number($('calGamma').value)*100),photoCorrection:$('calCorrect').checked}});}
async function resetColorCalibration(){await command('setColorCalibration',{value:{r:100,g:100,b:100,gamma100:180,photoCorrection:false}});await refreshState();}
async function toggleDepthTest(){await command('setDepthComparison',{value:!state.depthComparisonEnabled});await refreshState()}
// WIFI_CONTROLS_START — explicit acknowledgements, durable inline feedback.
let wifiBusy=false;
function wifiMessage(text,ok=true){$('wifiMessage').textContent=text;$('wifiMessage').style.color=ok?'var(--text)':'var(--bad)';}
function renderSavedNetworks(){const select=$('savedWifi'),networks=state.savedNetworks||[],old=select.value;select.replaceChildren();for(const n of networks){const option=document.createElement('option');option.value=String(n.index);option.textContent=n.ssid+(n.connected?' · connected':'')+(n.preferred?' · preferred':'');select.appendChild(option)}if(networks.some(n=>String(n.index)===old))select.value=old;else if(networks.length)select.value=String((networks.find(n=>n.connected)||networks.find(n=>n.preferred)||networks[0]).index);if(!networks.length){const option=document.createElement('option');option.textContent='No saved networks';option.value='';select.appendChild(option)}select.disabled=wifiBusy||!networks.length;$('connectWifi').disabled=wifiBusy||!networks.length;$('saveWifi').disabled=wifiBusy;$('wifiConnection').textContent=state.networkMode==='ap'?'Setup hotspot active.':state.wifiConnected===false?'Wi-Fi disconnected; waiting for recovery.':'Connected to '+(state.ssid||'Wi-Fi')+'.';}
async function wifiRequest(url,body){const controller=new AbortController(),timeout=setTimeout(()=>controller.abort(),12000);try{const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body),signal:controller.signal});let d;try{d=await r.json()}catch(e){throw new Error('No confirmation received. Refresh before retrying.')}if(!r.ok||d.success!==true)throw new Error(d.message||'Request was not confirmed');return d}catch(e){if(e.name==='AbortError')throw new Error('No response within 12 seconds. Refresh to check whether the network was saved.');throw e}finally{clearTimeout(timeout)}}
async function addNetwork(){if(wifiBusy)return;const ssid=$('newSsid').value.trim(),pass=$('newPass').value;if(!ssid){wifiMessage('Enter a network name / SSID.',false);return}wifiBusy=true;renderSavedNetworks();wifiMessage('Saving network…');try{await wifiRequest('/wifi/add',{ssid,password:pass,restart:false});$('newSsid').value='';$('newPass').value='';wifiMessage('Saved “'+ssid+'” on the matrix. Select it above and press Connect selected to switch.');await refreshState();const n=(state.savedNetworks||[]).find(n=>n.ssid===ssid);if(n)$('savedWifi').value=String(n.index)}catch(e){wifiMessage('Save not confirmed: '+e.message,false)}finally{wifiBusy=false;renderSavedNetworks()}}
async function connectSavedNetwork(){if(wifiBusy)return;const n=(state.savedNetworks||[]).find(n=>String(n.index)===$('savedWifi').value);if(!n){wifiMessage('Choose a saved network first.',false);return}wifiBusy=true;renderSavedNetworks();wifiMessage('Requesting connection to “'+n.ssid+'”…');try{await wifiRequest('/control',{command:'connectWifi',value:n.index});wifiMessage('Switch accepted. Matrix restarting: join “'+n.ssid+'” on your phone/computer, then reopen http://'+state.hostname+'.local/. If unavailable, the matrix tries the other saved networks, then its setup hotspot.');setTimeout(()=>{wifiBusy=false;refreshState()},12000)}catch(e){wifiBusy=false;renderSavedNetworks();wifiMessage('Switch not confirmed: '+e.message+' If the page disconnected, reconnect to the selected Wi-Fi or setup hotspot.',false)}}
// WIFI_CONTROLS_END
async function resetWifi(){if(!confirm('Forget every stored Wi-Fi network and restart in setup mode?'))return;try{await api('/wifi/reset',{method:'POST'});toast('Restarting in setup mode')}catch(e){toast(e.message,false)}}
window.addEventListener('load',()=>{setTheme(document.documentElement.dataset.theme,false);refreshState();refreshDiagnostics();requestAnimationFrame(animatePatternPreviews);setInterval(refreshDiagnostics,1000)});
</script>
</body>
</html>
)HTML";

const char SETUP_HTML[] PROGMEM = R"HTML(
<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="theme-color" content="#090b12"><title>Matrix Wi-Fi Setup</title><style>
:root{color-scheme:dark;--line:rgba(255,255,255,.1);--a:#7c5cff;--b:#18d5c2;--muted:#9aa3b7}*{box-sizing:border-box}body{margin:0;min-height:100vh;display:grid;place-items:center;padding:18px;font-family:Inter,system-ui,-apple-system,"Segoe UI",sans-serif;color:#f5f7ff;background:radial-gradient(circle at 15% 10%,rgba(124,92,255,.25),transparent 34%),radial-gradient(circle at 90% 10%,rgba(24,213,194,.16),transparent 30%),#080a10}.card{width:min(520px,100%);padding:28px;border:1px solid var(--line);border-radius:24px;background:rgba(20,24,36,.88);box-shadow:0 25px 90px rgba(0,0,0,.5);backdrop-filter:blur(18px)}.icon{width:56px;height:56px;border-radius:18px;background:linear-gradient(135deg,var(--a),var(--b));display:grid;place-items:center;font-size:25px;margin-bottom:18px}h1{font-size:1.55rem;margin:0 0 8px}p{color:var(--muted);line-height:1.55;margin:0 0 22px}.field{margin:12px 0}label{display:block;font-size:.78rem;color:var(--muted);margin:0 0 7px}input,select{width:100%;padding:13px 14px;border-radius:12px;border:1px solid var(--line);background:#101522;color:white;outline:none}input:focus,select:focus{border-color:var(--a);box-shadow:0 0 0 3px rgba(124,92,255,.13)}button{width:100%;padding:13px;border:0;border-radius:12px;background:linear-gradient(135deg,var(--a),#5c79ff);color:white;font-weight:800;cursor:pointer;margin-top:9px}.secondary{background:rgba(255,255,255,.06);border:1px solid var(--line)}.note{margin-top:18px;padding:13px;border:1px solid var(--line);border-radius:13px;font-size:.78rem;color:var(--muted);line-height:1.55}.msg{min-height:22px;margin-top:13px;font-size:.82rem;color:var(--b)}
</style></head><body><main class="card"><div class="icon">⌁</div><h1>Connect your matrix</h1><p>No saved Wi-Fi network was reachable, so the ESP32 opened its own setup network. Choose a network, enter its password, and the controller will restart.</p><div class="field"><label>Wi-Fi network</label><select id="ssid"><option value="">Scanning…</option></select></div><div class="field"><label>Or enter SSID manually</label><input id="manual" maxlength="32" placeholder="Network name"></div><div class="field"><label>Password</label><input id="pass" type="password" maxlength="64" placeholder="Wi-Fi password"></div><button onclick="save()">Save and restart</button><button class="secondary" onclick="scan()">Scan again</button><div id="msg" class="msg"></div><div class="note">Use the matrix setup network shown in Wi-Fi settings.<br>Local setup address: <strong>192.168.4.1</strong><br>The matrix animations continue running during setup.</div></main><script>
const $=x=>document.getElementById(x);async function scan(){const m=$('msg'),s=$('ssid');m.textContent='Scanning nearby networks…';try{const r=await fetch('/wifi/scan');const d=await r.json();s.innerHTML='<option value="">Select a network</option>';d.networks.forEach(n=>{const o=document.createElement('option');o.value=n.ssid;o.textContent=n.ssid+' ('+n.rssi+' dBm)'+(n.secure?' 🔒':'');s.appendChild(o)});m.textContent=d.networks.length?'Select a network or enter one manually.':'No networks found. You can enter the SSID manually.'}catch(e){m.textContent='Scan failed. Enter the SSID manually.'}}async function save(){const ssid=$('manual').value.trim()||$('ssid').value,pass=$('pass').value,m=$('msg');if(!ssid){m.textContent='Please choose or enter an SSID.';return}m.textContent='Saving network…';try{const r=await fetch('/wifi/add',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid,password:pass,restart:true})});const d=await r.json();if(!r.ok)throw new Error(d.message||'Could not save');m.textContent='Saved. The ESP32 is restarting…'}catch(e){m.textContent=e.message}}window.addEventListener('load',scan);
</script></body></html>
)HTML";


// -----------------------------------------------------------------------------
// JSON helpers and web handlers
// -----------------------------------------------------------------------------
float clampFloat(float value, float minimum, float maximum) {
  if (!isfinite(value)) return minimum;
  if (value < minimum) return minimum;
  if (value > maximum) return maximum;
  return value;
}

int clampInt(int value, int minimum, int maximum) {
  if (value < minimum) return minimum;
  if (value > maximum) return maximum;
  return value;
}

void addCurrentParamsToJson(JsonObject obj) {
  switch (currentAnimation) {
    case STARFIELD:
      obj["b"] = sfParams.b; obj["n"] = sfParams.n; obj["as"] = sfParams.as; obj["s"] = sfParams.s; obj["w"] = sfParams.w;
      obj["lfo_target"] = sfParams.lfo_target; obj["lfo_speed"] = sfParams.lfo_speed; obj["lfo_amount"] = sfParams.lfo_amount; break;
    case WARPED_CHECKERBOARD:
      obj["b"] = wcParams.b; obj["as"] = wcParams.as; obj["ws"] = wcParams.ws; obj["d"] = wcParams.d; obj["wsp"] = wcParams.wsp;
      obj["lfo_target"] = wcParams.lfo_target; obj["lfo_speed"] = wcParams.lfo_speed; obj["lfo_amount"] = wcParams.lfo_amount; break;
    case MOIRE_RINGS:
      obj["b"] = mrParams.b; obj["as"] = mrParams.as; obj["orb"] = mrParams.orb; obj["rt"] = mrParams.rt; obj["es"] = mrParams.es;
      obj["lfo_target"] = mrParams.lfo_target; obj["lfo_speed"] = mrParams.lfo_speed; obj["lfo_amount"] = mrParams.lfo_amount; break;
    case MOIRE_LINES:
      obj["b"] = mlParams.b; obj["as"] = mlParams.as; obj["rs1"] = mlParams.rs1; obj["rs2"] = mlParams.rs2; obj["lf"] = mlParams.lf;
      obj["lfo_target"] = mlParams.lfo_target; obj["lfo_speed"] = mlParams.lfo_speed; obj["lfo_amount"] = mlParams.lfo_amount; break;
    case WARPING_CHECKERS:
      obj["b"] = wchParams.b; obj["as"] = wchParams.as; obj["os"] = wchParams.os; obj["rt"] = wchParams.rt; obj["es"] = wchParams.es;
      obj["lfo_target"] = wchParams.lfo_target; obj["lfo_speed"] = wchParams.lfo_speed; obj["lfo_amount"] = wchParams.lfo_amount; break;
    case SHOCKWAVE:
      obj["b"] = swParams.b; obj["as"] = swParams.as; obj["ws"] = swParams.ws; obj["wt"] = swParams.wt;
      obj["lfo_target"] = swParams.lfo_target; obj["lfo_speed"] = swParams.lfo_speed; obj["lfo_amount"] = swParams.lfo_amount; break;
    case WARP_CORRIDORS:
      obj["b"] = wcoParams.b; obj["as"] = wcoParams.as; obj["rs"] = wcoParams.rs; obj["ws"] = wcoParams.ws; obj["bf"] = wcoParams.bf;
      obj["lfo_target"] = wcoParams.lfo_target; obj["lfo_speed"] = wcoParams.lfo_speed; obj["lfo_amount"] = wcoParams.lfo_amount; break;
    case PIXEL_AVALANCHE:
      obj["b"] = paParams.b; obj["sr"] = paParams.sr; obj["as"] = paParams.as;
      obj["lfo_target"] = paParams.lfo_target; obj["lfo_speed"] = paParams.lfo_speed; obj["lfo_amount"] = paParams.lfo_amount; break;
    case KALEIDOSCOPE:
      obj["b"] = kdParams.b; obj["p"] = kdParams.p; obj["as"] = kdParams.as; obj["rs"] = kdParams.rs; obj["es"] = kdParams.es;
      obj["lfo_target"] = kdParams.lfo_target; obj["lfo_speed"] = kdParams.lfo_speed; obj["lfo_amount"] = kdParams.lfo_amount; break;
    case BAYER_FLOW:
      obj["b"] = bdParams.b; obj["as"] = bdParams.as; obj["scale"] = bdParams.scale; obj["contrast"] = bdParams.contrast; obj["drift"] = bdParams.drift;
      obj["lfo_target"] = bdParams.lfo_target; obj["lfo_speed"] = bdParams.lfo_speed; obj["lfo_amount"] = bdParams.lfo_amount; break;
    case FLOYD_FLOW:
      obj["b"] = fdParams.b; obj["as"] = fdParams.as; obj["levels"] = fdParams.levels; obj["scale"] = fdParams.scale; obj["warp"] = fdParams.warp;
      obj["lfo_target"] = fdParams.lfo_target; obj["lfo_speed"] = fdParams.lfo_speed; obj["lfo_amount"] = fdParams.lfo_amount; break;
    case SNAKE:
      obj["b"] = snParams.b; obj["speed"] = snParams.speed; obj["cell"] = snParams.cell; obj["length"] = snParams.length; obj["steering"] = snParams.steering;
      obj["lfo_target"] = snParams.lfo_target; obj["lfo_speed"] = snParams.lfo_speed; obj["lfo_amount"] = snParams.lfo_amount; break;
    case SQUARES:
      obj["b"] = sqParams.b; obj["as"] = sqParams.as; obj["count"] = sqParams.count; obj["thickness"] = sqParams.thickness; obj["spacing"] = sqParams.spacing; obj["variance"] = sqParams.variance;
      obj["lfo_target"] = sqParams.lfo_target; obj["lfo_speed"] = sqParams.lfo_speed; obj["lfo_amount"] = sqParams.lfo_amount; break;
    case PERSPECTIVE_FLOW:
      obj["b"] = pfParams.b; obj["density"] = pfParams.density; obj["amplitude"] = pfParams.amplitude; obj["as"] = pfParams.as; obj["detail"] = pfParams.detail;
      obj["lfo_target"] = pfParams.lfo_target; obj["lfo_speed"] = pfParams.lfo_speed; obj["lfo_amount"] = pfParams.lfo_amount; break;
    case FLOW_SPHERE:
      obj["b"] = fsParams.b; obj["speed"] = fsParams.speed; obj["scale"] = fsParams.scale; obj["density"] = fsParams.density; obj["radius"] = fsParams.radius;
      obj["lfo_target"] = fsParams.lfo_target; obj["lfo_speed"] = fsParams.lfo_speed; obj["lfo_amount"] = fsParams.lfo_amount; break;
    case FLOW_FIELD_2D:
      obj["b"] = f2Params.b; obj["speed"] = f2Params.speed; obj["scale"] = f2Params.scale; obj["density"] = f2Params.density; obj["curl"] = f2Params.curl; obj["trails"] = f2Params.trails;
      obj["lfo_target"] = f2Params.lfo_target; obj["lfo_speed"] = f2Params.lfo_speed; obj["lfo_amount"] = f2Params.lfo_amount; break;
    case PATTERNFLOW_COLLECTION:
      obj["b"] = pfcParams.b; obj["variant"] = pfcParams.variant;
      obj["c0"] = pfcParams.control[0]; obj["c1"] = pfcParams.control[1];
      obj["c2"] = pfcParams.control[2]; obj["c3"] = pfcParams.control[3];
      obj["lfo_target"] = pfcParams.lfo_target; obj["lfo_speed"] = pfcParams.lfo_speed; obj["lfo_amount"] = pfcParams.lfo_amount; break;
  }
  const PatternStyleParams& style = styleParams[(int)currentAnimation];
  obj["zoom"] = style.zoom;
  obj["rotation"] = style.rotation;
  obj["offsetX"] = style.offsetX;
  obj["offsetY"] = style.offsetY;
  obj["symmetry"] = style.symmetry;
  const ColorParams& color = colorParams[(int)currentAnimation];
  obj["hue"] = color.hue;
  obj["spread"] = color.spread;
  obj["palette"] = color.palette;
  obj["globalHueShift"] = colorTuning[(int)currentAnimation].hueShift;
  obj["globalSaturation"] = colorTuning[(int)currentAnimation].saturation;
  for (int colorIndex = 0; colorIndex < 4; colorIndex++) {
    char key[8];
    char hex[8];
    snprintf(key, sizeof(key), "color%d", colorIndex);
    snprintf(hex, sizeof(hex), "#%06lX",
             (unsigned long)(paletteStops[(int)currentAnimation]
               [constrain((int)color.palette, 0, PALETTE_COUNT-1)][colorIndex] & 0xFFFFFF));
    obj[key] = hex;
  }
}

bool setAnimationParam(int animIndex, const char* name, JsonVariantConst value) {
  if (!isAvailableEngine(animIndex) || name == nullptr) return false;

  const float f = value.as<float>();
  const int i = value.as<int>();
  bool changed = true;

  if (!strcmp(name, "hue")) {
    colorParams[animIndex].hue = clampInt(i, 0, 359);
    huePaletteAnimation = -1;
    return true;
  }
  if (!strcmp(name, "spread")) {
    colorParams[animIndex].spread = clampInt(i, 0, 360);
    huePaletteAnimation = -1;
    return true;
  }
  if (!strcmp(name, "palette")) {
    colorParams[animIndex].palette = clampInt(i, 0, PALETTE_COUNT-1);
    huePaletteAnimation = -1;
    return true;
  }
  if (!strcmp(name, "globalHueShift")) {
    colorTuning[animIndex].hueShift = clampInt(i, -180, 180);
    huePaletteAnimation = -1;
    return true;
  }
  if (!strcmp(name, "globalSaturation")) {
    colorTuning[animIndex].saturation = clampInt(i, 0, 200);
    huePaletteAnimation = -1;
    return true;
  }
  if (!strcmp(name, "zoom")) {
    styleParams[animIndex].zoom = clampFloat(f, 0.5f, 2.0f);
    return true;
  }
  if (!strcmp(name, "rotation")) {
    styleParams[animIndex].rotation = clampFloat(f, -180.0f, 180.0f);
    return true;
  }
  if (!strcmp(name, "offsetX")) {
    styleParams[animIndex].offsetX = clampInt(i, -32, 32);
    return true;
  }
  if (!strcmp(name, "offsetY")) {
    styleParams[animIndex].offsetY = clampInt(i, -32, 32);
    return true;
  }
  if (!strcmp(name, "symmetry")) {
    styleParams[animIndex].symmetry = clampInt(i, 0, 3);
    return true;
  }
  if (!strncmp(name, "color", 5) && strlen(name) == 6 &&
      name[5] >= '0' && name[5] <= '3') {
    const char* raw = value.as<const char*>();
    if (!raw || raw[0] != '#' || strlen(raw) != 7) return false;
    const int palette = constrain((int)colorParams[animIndex].palette, 0, PALETTE_COUNT-1);
    paletteStops[animIndex][palette][name[5] - '0'] =
      strtoul(raw + 1, nullptr, 16) & 0xFFFFFF;
    huePaletteAnimation = -1;
    return true;
  }

  switch (animIndex) {
    case STARFIELD:
      if (!strcmp(name,"b")) sfParams.b=clampInt(i,0,15); else if(!strcmp(name,"as")) sfParams.as=clampFloat(f,.1f,3); else if(!strcmp(name,"n")) sfParams.n=clampInt(i,10,MAX_STARS); else if(!strcmp(name,"s")) sfParams.s=clampFloat(f,2,10); else if(!strcmp(name,"w")) sfParams.w=clampFloat(f,.05f,1); else if(!strcmp(name,"lfo_target")) sfParams.lfo_target=clampInt(i,0,4); else if(!strcmp(name,"lfo_speed")) sfParams.lfo_speed=clampFloat(f,.05f,10); else if(!strcmp(name,"lfo_amount")) sfParams.lfo_amount=clampFloat(f,0,1); else changed=false; break;
    case WARPED_CHECKERBOARD:
      if (!strcmp(name,"b")) wcParams.b=clampInt(i,0,15); else if(!strcmp(name,"as")) wcParams.as=clampFloat(f,.1f,3); else if(!strcmp(name,"ws")) wcParams.ws=clampFloat(f,2,10); else if(!strcmp(name,"d")) wcParams.d=clampFloat(f,1,8); else if(!strcmp(name,"wsp")) wcParams.wsp=clampFloat(f,1,8); else if(!strcmp(name,"lfo_target")) wcParams.lfo_target=clampInt(i,0,4); else if(!strcmp(name,"lfo_speed")) wcParams.lfo_speed=clampFloat(f,.05f,10); else if(!strcmp(name,"lfo_amount")) wcParams.lfo_amount=clampFloat(f,0,1); else changed=false; break;
    case MOIRE_RINGS:
      if (!strcmp(name,"b")) mrParams.b=clampInt(i,0,15); else if(!strcmp(name,"as")) mrParams.as=clampFloat(f,.1f,3); else if(!strcmp(name,"orb")) mrParams.orb=clampFloat(f,0,16); else if(!strcmp(name,"rt")) mrParams.rt=clampFloat(f,2,16); else if(!strcmp(name,"es")) mrParams.es=clampFloat(f,.1f,5); else if(!strcmp(name,"lfo_target")) mrParams.lfo_target=clampInt(i,0,4); else if(!strcmp(name,"lfo_speed")) mrParams.lfo_speed=clampFloat(f,.05f,10); else if(!strcmp(name,"lfo_amount")) mrParams.lfo_amount=clampFloat(f,0,1); else changed=false; break;
    case MOIRE_LINES:
      if (!strcmp(name,"b")) mlParams.b=clampInt(i,0,15); else if(!strcmp(name,"as")) mlParams.as=clampFloat(f,.1f,3); else if(!strcmp(name,"rs1")) mlParams.rs1=clampFloat(f,.1f,3); else if(!strcmp(name,"rs2")) mlParams.rs2=clampFloat(f,.1f,3); else if(!strcmp(name,"lf")) mlParams.lf=clampFloat(f,.1f,2); else if(!strcmp(name,"lfo_target")) mlParams.lfo_target=clampInt(i,0,4); else if(!strcmp(name,"lfo_speed")) mlParams.lfo_speed=clampFloat(f,.05f,10); else if(!strcmp(name,"lfo_amount")) mlParams.lfo_amount=clampFloat(f,0,1); else changed=false; break;
    case WARPING_CHECKERS:
      if (!strcmp(name,"b")) wchParams.b=clampInt(i,0,15); else if(!strcmp(name,"as")) wchParams.as=clampFloat(f,.1f,3); else if(!strcmp(name,"os")) wchParams.os=clampFloat(f,.1f,2); else if(!strcmp(name,"rt")) wchParams.rt=clampFloat(f,1,8); else if(!strcmp(name,"es")) wchParams.es=clampFloat(f,.1f,2); else if(!strcmp(name,"lfo_target")) wchParams.lfo_target=clampInt(i,0,4); else if(!strcmp(name,"lfo_speed")) wchParams.lfo_speed=clampFloat(f,.05f,10); else if(!strcmp(name,"lfo_amount")) wchParams.lfo_amount=clampFloat(f,0,1); else changed=false; break;
    case SHOCKWAVE:
      if (!strcmp(name,"b")) swParams.b=clampInt(i,0,15); else if(!strcmp(name,"as")) swParams.as=clampFloat(f,.1f,3); else if(!strcmp(name,"ws")) swParams.ws=clampFloat(f,.1f,5); else if(!strcmp(name,"wt")) swParams.wt=clampFloat(f,.1f,5); else if(!strcmp(name,"lfo_target")) swParams.lfo_target=clampInt(i,0,3); else if(!strcmp(name,"lfo_speed")) swParams.lfo_speed=clampFloat(f,.05f,10); else if(!strcmp(name,"lfo_amount")) swParams.lfo_amount=clampFloat(f,0,1); else changed=false; break;
    case WARP_CORRIDORS:
      if (!strcmp(name,"b")) wcoParams.b=clampInt(i,0,15); else if(!strcmp(name,"as")) wcoParams.as=clampFloat(f,.1f,3); else if(!strcmp(name,"rs")) wcoParams.rs=clampFloat(f,.1f,2); else if(!strcmp(name,"ws")) wcoParams.ws=clampFloat(f,.1f,2); else if(!strcmp(name,"bf")) wcoParams.bf=clampFloat(f,.1f,2); else if(!strcmp(name,"lfo_target")) wcoParams.lfo_target=clampInt(i,0,4); else if(!strcmp(name,"lfo_speed")) wcoParams.lfo_speed=clampFloat(f,.05f,10); else if(!strcmp(name,"lfo_amount")) wcoParams.lfo_amount=clampFloat(f,0,1); else changed=false; break;
    case PIXEL_AVALANCHE:
      if (!strcmp(name,"b")) paParams.b=clampInt(i,0,15); else if(!strcmp(name,"as")) paParams.as=clampFloat(f,.1f,3); else if(!strcmp(name,"sr")) paParams.sr=clampInt(i,1,10); else if(!strcmp(name,"lfo_target")) paParams.lfo_target=clampInt(i,0,2); else if(!strcmp(name,"lfo_speed")) paParams.lfo_speed=clampFloat(f,.05f,10); else if(!strcmp(name,"lfo_amount")) paParams.lfo_amount=clampFloat(f,0,1); else changed=false; break;
    case KALEIDOSCOPE:
      if (!strcmp(name,"b")) kdParams.b=clampInt(i,0,15); else if(!strcmp(name,"as")) kdParams.as=clampFloat(f,.1f,3); else if(!strcmp(name,"p")) kdParams.p=clampInt(i,1,15); else if(!strcmp(name,"rs")) kdParams.rs=clampFloat(f,.1f,2); else if(!strcmp(name,"es")) kdParams.es=clampFloat(f,.1f,2); else if(!strcmp(name,"lfo_target")) kdParams.lfo_target=clampInt(i,0,4); else if(!strcmp(name,"lfo_speed")) kdParams.lfo_speed=clampFloat(f,.05f,10); else if(!strcmp(name,"lfo_amount")) kdParams.lfo_amount=clampFloat(f,0,1); else changed=false; break;
    case BAYER_FLOW:
      if (!strcmp(name,"b")) bdParams.b=clampInt(i,0,15); else if(!strcmp(name,"as")) bdParams.as=clampFloat(f,.1f,3); else if(!strcmp(name,"scale")) bdParams.scale=clampInt(i,1,16); else if(!strcmp(name,"contrast")) bdParams.contrast=clampInt(i,1,8); else if(!strcmp(name,"drift")) bdParams.drift=clampFloat(f,.1f,3); else if(!strcmp(name,"lfo_target")) bdParams.lfo_target=clampInt(i,0,4); else if(!strcmp(name,"lfo_speed")) bdParams.lfo_speed=clampFloat(f,.05f,10); else if(!strcmp(name,"lfo_amount")) bdParams.lfo_amount=clampFloat(f,0,1); else changed=false; break;
    case FLOYD_FLOW:
      if (!strcmp(name,"b")) fdParams.b=clampInt(i,0,15); else if(!strcmp(name,"as")) fdParams.as=clampFloat(f,.1f,3); else if(!strcmp(name,"levels")) fdParams.levels=clampInt(i,2,6); else if(!strcmp(name,"scale")) fdParams.scale=clampInt(i,1,16); else if(!strcmp(name,"warp")) fdParams.warp=clampFloat(f,.1f,3); else if(!strcmp(name,"lfo_target")) fdParams.lfo_target=clampInt(i,0,4); else if(!strcmp(name,"lfo_speed")) fdParams.lfo_speed=clampFloat(f,.05f,10); else if(!strcmp(name,"lfo_amount")) fdParams.lfo_amount=clampFloat(f,0,1); else changed=false; break;
    case SNAKE:
      if (!strcmp(name,"b")) snParams.b=clampInt(i,0,15); else if(!strcmp(name,"speed")) snParams.speed=clampFloat(f,1,20); else if(!strcmp(name,"length")) snParams.length=clampInt(i,8,300); else if(!strcmp(name,"steering")) snParams.steering=clampFloat(f,0,1); else if(!strcmp(name,"lfo_target")) snParams.lfo_target=clampInt(i,0,3); else if(!strcmp(name,"lfo_speed")) snParams.lfo_speed=clampFloat(f,.05f,10); else if(!strcmp(name,"lfo_amount")) snParams.lfo_amount=clampFloat(f,0,1); else changed=false; break;
    case SQUARES:
      if (!strcmp(name,"b")) sqParams.b=clampInt(i,0,15); else if(!strcmp(name,"as")) sqParams.as=clampFloat(f,.1f,4); else if(!strcmp(name,"count")) sqParams.count=clampInt(i,2,20); else if(!strcmp(name,"thickness")) sqParams.thickness=clampInt(i,1,4); else if(!strcmp(name,"spacing")) sqParams.spacing=clampFloat(f,.2f,2); else if(!strcmp(name,"variance")) sqParams.variance=clampFloat(f,0,.5f); else if(!strcmp(name,"lfo_target")) sqParams.lfo_target=clampInt(i,0,4); else if(!strcmp(name,"lfo_speed")) sqParams.lfo_speed=clampFloat(f,.05f,10); else if(!strcmp(name,"lfo_amount")) sqParams.lfo_amount=clampFloat(f,0,1); else changed=false; break;
    case PERSPECTIVE_FLOW:
      if (!strcmp(name,"b")) pfParams.b=clampInt(i,0,15); else if(!strcmp(name,"density")) pfParams.density=clampFloat(f,2.3f,5); else if(!strcmp(name,"amplitude")) pfParams.amplitude=clampFloat(f,15,60); else if(!strcmp(name,"as")) pfParams.as=clampFloat(f,.1f,5); else if(!strcmp(name,"detail")) pfParams.detail=clampInt(i,3,12); else if(!strcmp(name,"lfo_target")) pfParams.lfo_target=clampInt(i,0,4); else if(!strcmp(name,"lfo_speed")) pfParams.lfo_speed=clampFloat(f,.05f,10); else if(!strcmp(name,"lfo_amount")) pfParams.lfo_amount=clampFloat(f,0,1); else changed=false; break;
    case FLOW_SPHERE:
      if (!strcmp(name,"b")) fsParams.b=clampInt(i,0,15); else if(!strcmp(name,"speed")) fsParams.speed=clampFloat(f,.05f,6); else if(!strcmp(name,"scale")) fsParams.scale=clampFloat(f,.5f,4); else if(!strcmp(name,"density")) fsParams.density=clampFloat(f,.1f,1); else if(!strcmp(name,"radius")) fsParams.radius=clampFloat(f,14,31); else if(!strcmp(name,"lfo_target")) fsParams.lfo_target=clampInt(i,0,4); else if(!strcmp(name,"lfo_speed")) fsParams.lfo_speed=clampFloat(f,.05f,10); else if(!strcmp(name,"lfo_amount")) fsParams.lfo_amount=clampFloat(f,0,1); else changed=false; break;
    case FLOW_FIELD_2D:
      if (!strcmp(name,"b")) f2Params.b=clampInt(i,0,15); else if(!strcmp(name,"speed")) f2Params.speed=clampFloat(f,.05f,8); else if(!strcmp(name,"scale")) f2Params.scale=clampFloat(f,.3f,4); else if(!strcmp(name,"density")) f2Params.density=clampFloat(f,.05f,1); else if(!strcmp(name,"curl")) f2Params.curl=clampFloat(f,.1f,3); else if(!strcmp(name,"trails")) f2Params.trails=clampInt(i,1,8); else if(!strcmp(name,"lfo_target")) f2Params.lfo_target=clampInt(i,0,5); else if(!strcmp(name,"lfo_speed")) f2Params.lfo_speed=clampFloat(f,.05f,10); else if(!strcmp(name,"lfo_amount")) f2Params.lfo_amount=clampFloat(f,0,1); else changed=false; break;
    case PATTERNFLOW_COLLECTION:
      if (!strcmp(name,"b")) pfcParams.b=clampInt(i,0,15);
      else if(!strcmp(name,"variant")) {
        if(!isAvailableVariant(i)||isPatternArchived(PATTERNFLOW_COLLECTION,i))changed=false;
        else loadVariantPatch(i,variantDefaults[i]);
      }
      else if(name[0]=='c' && name[1]>='0' && name[1]<='3' && name[2]=='\0') pfcParams.control[name[1]-'0']=clampInt(i,0,100);
      else if(!strcmp(name,"lfo_target")) pfcParams.lfo_target=clampInt(i,0,4);
      else if(!strcmp(name,"lfo_speed")) pfcParams.lfo_speed=clampFloat(f,.05f,10);
      else if(!strcmp(name,"lfo_amount")) pfcParams.lfo_amount=clampFloat(f,0,1);
      else changed=false;
      break;
  }

  if (changed && !strcmp(name, "b") && animIndex == currentAnimation) applyCurrentBrightness();
  return changed;
}

void sendJson(bool success, const String& message, int status = 200) {
  DynamicJsonDocument doc(256);
  doc["success"] = success;
  doc["message"] = message;
  String response;
  serializeJson(doc, response);
  server.send(status, "application/json", response);
}

void handleRoot() {
  // The complete dashboard also contains Wi-Fi setup controls, so it remains
  // useful in station, setup-hotspot, and USB-recovery workflows.
  server.send_P(200, "text/html; charset=utf-8", DASHBOARD_HTML);
}

void handlePreviewAtlas() {
  server.sendHeader("Cache-Control", "public, max-age=86400");
  server.send_P(200, "image/png",
                reinterpret_cast<const char*>(PATTERN_PREVIEW_ATLAS),
                PATTERN_PREVIEW_ATLAS_SIZE);
}

void handleSetupPage() {
  server.send_P(200, "text/html; charset=utf-8", SETUP_HTML);
}

String buildControlJson() {
  DynamicJsonDocument doc(14336);
  doc["version"] = "2.0";
  doc["libraryRevision"] = libraryRevision;
  doc["patternQuality"] = currentAnimation == PATTERNFLOW_COLLECTION
    ? patternflowQuality[constrain(pfcParams.variant,0,PATTERNFLOW_PATTERN_COUNT-1)] : 0;
  doc["previewFrames"]=PATTERN_PREVIEW_FRAMES;
  doc["previewWidth"]=PATTERN_PREVIEW_WIDTH;doc["previewHeight"]=PATTERN_PREVIEW_HEIGHT;
  doc["previewIntervalMs"]=PATTERN_PREVIEW_INTERVAL_MS;
  doc["previewAtlasKey"]=PATTERN_PREVIEW_ATLAS_KEY;
  JsonArray catalog=doc.createNestedArray("catalog");
  for(int i=0;i<CORE_PATTERN_COUNT;i++){JsonObject item=catalog.createNestedObject();int id=CORE_PATTERN_IDS[i];item["name"]=animationNames[id];item["engine"]=id;item["variant"]=nullptr;}
  for(int j=0;j<COLLECTION_PATTERN_COUNT;j++){int v=COLLECTION_PATTERN_IDS[j];JsonObject item=catalog.createNestedObject();item["name"]=PATTERNFLOW_METADATA[v].name;item["engine"]=16;item["variant"]=v;}
  JsonArray bookmarks = doc.createNestedArray("bookmarks");
  for (uint8_t i=0; i<GALLERY_PATTERN_COUNT; i++) bookmarks.add(patternBookmarks[i]);
  doc["success"] = true;
  doc["currentPattern"] = (int)currentAnimation;
  doc["stateRevision"] = stateRevision;
  doc["targetFPS"] = targetFPS;
  doc["autoCycleEnabled"] = autoCycleEnabled;
  doc["cycleInterval"] = autoCycleIntervalSecs;
  doc["resyncInterval"] = autoResyncInterval;
  doc["scheduleEnabled"] = scheduleEnabled;
  doc["scheduleStartMinutes"] = scheduleStartMinutes;
  doc["scheduleEndMinutes"] = scheduleEndMinutes;
  doc["scheduleFadeInSeconds"] = scheduleFadeInSeconds;
  doc["scheduleFadeOutSeconds"] = scheduleFadeOutSeconds;
  doc["scheduleOpacity"] = scheduleOpacity;
  doc["transitionOpacity"] = transitionOpacity;
  doc["deviceProfile"] = DEVICE_PROFILE_ID;
  doc["deviceName"] = DEVICE_DISPLAY_NAME;
  doc["projectName"] = "Matrix.hub";
  doc["projectAuthor"] = "Televator";
  doc["backend"] = DISPLAY_BACKEND_NAME;
  doc["displayWidth"] = SCREEN_WIDTH;
  doc["displayHeight"] = SCREEN_HEIGHT;
#if !defined(MATRIX_PROFILE_WS2812)
  doc["outputRotationDegrees"]=MATRIX_OUTPUT_ROTATION*90;
#endif
#if !defined(MATRIX_PROFILE_WS2812) && MATRIX_COLOR_DEPTH >= 4
  doc["depthComparisonAvailable"] = true;
#else
  doc["depthComparisonAvailable"] = false;
#endif
  JsonObject calibration=doc.createNestedObject("calibration");
  calibration["mode"]=calibrationMode;calibration["photo"]=calibrationPhoto;
  calibration["level"]=calibrationLevel;calibration["gamma100"]=photoGamma100;
  calibration["photoCorrection"]=photoCorrectionEnabled;
  calibration["r"]=panelBalance[0];calibration["g"]=panelBalance[1];calibration["b"]=panelBalance[2];
  calibration["bits"]=MATRIX_COLOR_DEPTH;
#if defined(MATRIX_PROFILE_WS2812)
  calibration["available"]=false;
#else
  calibration["available"]=true;
#endif
  doc["luaHelloAvailable"] = luaPattern.available();
  doc["luaHelloEnabled"] = luaHelloEnabled;
  doc["depthComparisonEnabled"] = depthComparisonEnabled;
  doc["luaMemoryUsed"] = luaPattern.memoryUsed();
  doc["luaMemoryLimit"] = luaPattern.memoryLimit();
  doc["luaError"] = luaPattern.lastError();
  doc["networkMode"] = isAPMode ? "ap" : "station";
  doc["hostname"] = HOSTNAME;
  doc["apSsid"] = SETUP_AP_SSID;
  doc["wifiConnected"]=WiFi.status()==WL_CONNECTED;
  JsonArray savedWifi=doc.createNestedArray("savedNetworks");
  Preferences wifiPrefs;
  if(wifiPrefs.begin("wifi-list",true)){
    int count=constrain(wifiPrefs.getInt("count",0),0,int(MAX_WIFI_NETWORKS));
    int preferred=wifiPrefs.getInt("preferred",-1);
    for(int i=0;i<count;i++){
      String ssid=wifiPrefs.getString(("ssid_"+String(i)).c_str());
      if(!ssid.length())continue;
      JsonObject network=savedWifi.createNestedObject();network["index"]=i;network["ssid"]=ssid;
      network["preferred"]=i==preferred;
      network["connected"]=!isAPMode&&WiFi.status()==WL_CONNECTED&&WiFi.SSID()==ssid;
    }
    wifiPrefs.end();
  }

  if (isAPMode) {
    doc["ip"] = WiFi.softAPIP().toString();
    doc["ssid"] = "";
    doc["rssi"] = 0;
  } else {
    doc["ip"] = WiFi.localIP().toString();
    doc["ssid"] = WiFi.SSID();
    doc["rssi"] = WiFi.RSSI();
  }

  JsonArray names = doc.createNestedArray("animationNames");
  for (int i = 0; i < NUM_ANIMATIONS; i++) names.add(animationNames[i]);

  JsonArray defaults = doc.createNestedArray("defaultPatches");
  for (int i = 0; i < NUM_ANIMATIONS; i++) defaults.add(defaultPatches[i]);

  JsonObject params = doc.createNestedObject("params");
  addCurrentParamsToJson(params);

  String response;
  response.reserve(4096);
  serializeJson(doc, response);
  return response;
}

void handleControlGet() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", buildControlJson());
}

String buildDiagnosticsJson() {
  StaticJsonDocument<2048> doc;
  doc["success"] = true;
  nvs_stats_t stats={};
  if(nvs_get_stats(nullptr,&stats)==ESP_OK){
    doc["settingsUsedEntries"]=stats.used_entries;
    doc["settingsFreeEntries"]=stats.free_entries;
  }
  const char* activePatternName = currentAnimation == PATTERNFLOW_COLLECTION
    ? PATTERNFLOW_METADATA[constrain(pfcParams.variant, 0,
                                    PATTERNFLOW_PATTERN_COUNT - 1)].name
    : animationNames[(int)currentAnimation];
  doc["pattern"] = calibrationMode ? "Color calibration" : depthComparisonEnabled
    ? "3-bit / 4-bit Test"
    : (luaHelloEnabled ? "Hello Lua" : activePatternName);
  doc["engine"] = luaHelloEnabled ? "lua" : "cpp";
  doc["targetFPS"] = targetFPS;
  doc["actualFPS"] = performance.actualFPS;
  doc["renderWidth"] = currentAnimation == PATTERNFLOW_COLLECTION
    ? min((int)SCREEN_WIDTH,patternflowQuality[pfcParams.variant]==0 ? 64 : patternflowRenderSize[pfcParams.variant]) : SCREEN_WIDTH;
  doc["renderHeight"] = currentAnimation == PATTERNFLOW_COLLECTION
    ? min((int)SCREEN_HEIGHT,patternflowQuality[pfcParams.variant]==0 ? 64 : patternflowRenderSize[pfcParams.variant]) : SCREEN_HEIGHT;
  doc["panelRefreshHz"] = performance.panelRefreshHz;
  doc["refreshLabel"] = DISPLAY_REFRESH_LABEL;
  doc["deviceProfile"] = DEVICE_PROFILE_ID;
  doc["backend"] = DISPLAY_BACKEND_NAME;
  doc["stateRevision"] = stateRevision;
  doc["averageRenderUs"] = performance.averageRenderUs;
  doc["averageDrawUs"] = performance.averageDrawUs;
  doc["averageShowUs"] = performance.averageShowUs;
  doc["worstRenderUs"] = performance.worstRenderUs;
  doc["lastRenderUs"] = performance.lastRenderUs;
  doc["overBudgetFrames"] = performance.overBudgetFrames;
  doc["sampleFrames"] = performance.sampleFrames;

  const uint32_t frameBudgetUs = targetFPS > 0 ? 1000000UL / targetFPS : 1000000UL;
  doc["frameBudgetUs"] = frameBudgetUs;
  doc["renderBudgetPercent"] = frameBudgetUs > 0
    ? performance.averageRenderUs * 100.0f / frameBudgetUs
    : 0.0f;
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["minimumFreeHeap"] = ESP.getMinFreeHeap();
  doc["largestFreeBlock"] = ESP.getMaxAllocHeap();
  doc["cpuMHz"] = ESP.getCpuFreqMHz();
  doc["colorDepth"] = MATRIX_COLOR_DEPTH;
#if !defined(MATRIX_PROFILE_WS2812)
  doc["outputRotationDegrees"]=MATRIX_OUTPUT_ROTATION*90;
#endif
#if defined(MATRIX_HUB75_DMA)
  doc["displayClockMHz"]=matrix.clockMHz();
  doc["timingProfile"]="experimental-4bit-dma-exact20";
  doc["refreshMeasured"]=matrix.counterReady();
  doc["outputDuty255"]=80;
#elif defined(MATRIX_TIMING_EXPERIMENT)
  doc["displayClockMHz"]=20;
  doc["timingProfile"]="experimental-4bit-20mhz";
#else
  doc["timingProfile"]="standard";
#endif
#if defined(MATRIX_PROFILE_WS2812)
  doc["patternStorageMounted"] = patternStorageReady;
  doc["patternStorageUsed"] = patternStorageReady ? LittleFS.usedBytes() : 0;
  doc["patternStorageTotal"] = patternStorageReady ? LittleFS.totalBytes() : 0;
#endif
  doc["scheduleOpacity"] = scheduleOpacity;
  doc["transitionOpacity"] = transitionOpacity;
  doc["uptimeMs"] = millis();

  doc["otaReady"] = otaStarted;

  String response;
  response.reserve(1280);
  serializeJson(doc, response);
  return response;
}

void handleDiagnosticsGet() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", buildDiagnosticsJson());
}

void savePatchToPreferences(int animIndex, int patchIndex) {
  preferences.begin("matrix-cfg", false);
  switch (animIndex) {
    case STARFIELD:           memcpy(&sfPatches[patchIndex], &sfParams, sizeof(sfParams)); preferences.putBytes("sf_p", sfPatches, sizeof(sfPatches)); break;
    case WARPED_CHECKERBOARD: memcpy(&wcPatches[patchIndex], &wcParams, sizeof(wcParams)); preferences.putBytes("wc_p", wcPatches, sizeof(wcPatches)); break;
    case MOIRE_RINGS:         memcpy(&mrPatches[patchIndex], &mrParams, sizeof(mrParams)); preferences.putBytes("mr_p", mrPatches, sizeof(mrPatches)); break;
    case MOIRE_LINES:         memcpy(&mlPatches[patchIndex], &mlParams, sizeof(mlParams)); preferences.putBytes("ml_p", mlPatches, sizeof(mlPatches)); break;
    case WARPING_CHECKERS:    memcpy(&wchPatches[patchIndex], &wchParams, sizeof(wchParams)); preferences.putBytes("wch_p", wchPatches, sizeof(wchPatches)); break;
    case SHOCKWAVE:           memcpy(&swPatches[patchIndex], &swParams, sizeof(swParams)); preferences.putBytes("sw_p", swPatches, sizeof(swPatches)); break;
    case WARP_CORRIDORS:      memcpy(&wcoPatches[patchIndex], &wcoParams, sizeof(wcoParams)); preferences.putBytes("wco_p", wcoPatches, sizeof(wcoPatches)); break;
    case PIXEL_AVALANCHE:     memcpy(&paPatches[patchIndex], &paParams, sizeof(paParams)); preferences.putBytes("pa_p", paPatches, sizeof(paPatches)); break;
    case KALEIDOSCOPE:        memcpy(&kdPatches[patchIndex], &kdParams, sizeof(kdParams)); preferences.putBytes("kd_p", kdPatches, sizeof(kdPatches)); break;
    case BAYER_FLOW:          memcpy(&bdPatches[patchIndex], &bdParams, sizeof(bdParams)); preferences.putBytes("bd_p", bdPatches, sizeof(bdPatches)); break;
    case FLOYD_FLOW:          memcpy(&fdPatches[patchIndex], &fdParams, sizeof(fdParams)); preferences.putBytes("fd_p", fdPatches, sizeof(fdPatches)); break;
    case SNAKE:               memcpy(&snPatches[patchIndex], &snParams, sizeof(snParams)); preferences.putBytes("sn_p", snPatches, sizeof(snPatches)); break;
    case SQUARES:             memcpy(&sqPatches[patchIndex], &sqParams, sizeof(sqParams)); preferences.putBytes("sq_p", sqPatches, sizeof(sqPatches)); break;
    case PERSPECTIVE_FLOW:    memcpy(&pfPatches[patchIndex], &pfParams, sizeof(pfParams)); preferences.putBytes("pf_p", pfPatches, sizeof(pfPatches)); break;
    case FLOW_SPHERE:         memcpy(&fsPatches[patchIndex], &fsParams, sizeof(fsParams)); preferences.putBytes("fs_p", fsPatches, sizeof(fsPatches)); break;
    case FLOW_FIELD_2D:       memcpy(&f2Patches[patchIndex], &f2Params, sizeof(f2Params)); preferences.putBytes("f2_p", f2Patches, sizeof(f2Patches)); break;
    case PATTERNFLOW_COLLECTION: memcpy(&pfcPatches[patchIndex], &pfcParams, sizeof(pfcParams)); preferences.putBytes("pfc_p", pfcPatches, sizeof(pfcPatches)); break;
  }
  colorPatches[animIndex][patchIndex] = colorParams[animIndex];
  preferences.putBytes("color_p", colorPatches, sizeof(colorPatches));
  colorTuningPatches[animIndex][patchIndex] = colorTuning[animIndex];
  preferences.putBytes("tuning_p", colorTuningPatches,
                       sizeof(colorTuningPatches));
  stylePatches[animIndex][patchIndex] = styleParams[animIndex];
  preferences.putBytes("style_p", stylePatches, sizeof(stylePatches));
  memcpy(customPalette[animIndex], paletteStops[animIndex][6],
         sizeof(customPalette[animIndex]));
  memcpy(customPalettePatches[animIndex][patchIndex], customPalette[animIndex],
         sizeof(customPalette[animIndex]));
  preferences.putBytes("custom_p", customPalettePatches,
                       sizeof(customPalettePatches));
  memcpy(paletteStopPatches[animIndex][patchIndex], paletteStops[animIndex],
         sizeof(paletteStops[animIndex]));
  writePalettePatches(preferences);

  preferences.end();
}

String makeCommandResultJson(bool success, const String& message) {
  DynamicJsonDocument result(256);
  result["success"] = success;
  result["message"] = message;
  String response;
  serializeJson(result, response);
  return response;
}

String processControlCommandJson(const String& body, int& status) {
  isUpdatingWeb = true;

  DynamicJsonDocument doc(1024);
  const DeserializationError error = deserializeJson(doc, body);
  if (error) {
    isUpdatingWeb = false;
    status = 400;
    return makeCommandResultJson(false, String("Invalid JSON: ") + error.c_str());
  }

  const char* commandName = doc["command"];
  if (!commandName) {
    isUpdatingWeb = false;
    status = 400;
    return makeCommandResultJson(false, "Missing command");
  }

  bool success = true;
  String message = "Updated";

  if (!strcmp(commandName,"saveWifiNetwork")) {
    JsonVariantConst value=doc["value"];
    success=value["ssid"].is<const char*>()&&value["password"].is<const char*>()&&!wifiRestartAt;
    if(success){String ssid=value["ssid"].as<String>();ssid.trim();success=addOrUpdateNetwork(ssid,value["password"].as<String>());}
    message=success?"Network saved and verified. Use Connect selected to switch.":"Could not verify network save; check SSID, storage or the eight-network limit.";
  } else if (!strcmp(commandName,"connectWifi")) {
    int index=doc["value"].as<int>();String ssid,password;
    success=doc["value"].is<int>()&&index>=0&&index<MAX_WIFI_NETWORKS&&!wifiRestartAt&&readWifiNetwork(index,ssid,password);
    if(success){Preferences store;success=store.begin("wifi-list",false);if(success){store.putInt("preferred",index);store.putBool("force_ap",false);success=store.getInt("preferred",-1)==index&&!store.getBool("force_ap",true);store.end();}}
    if(success)wifiRestartAt=millis()+1500;
    message=success?"Switch accepted. Restarting; the selected network will be tried first.":"Could not select saved network.";
  } else if (!strcmp(commandName, "setLibraryMetadata")) {
    success=saveLibraryMetadata(doc["value"].as<JsonObjectConst>(),message);
  } else if (!strcmp(commandName,"setPatternArchived")) {
    success=savePatternArchive(doc["value"].as<JsonObjectConst>(),message);
    if(success){
      if(isPatternArchived(currentAnimation,pfcParams.variant))advanceAutoCyclePattern();
      autoCycleTransitionPhase=0;transitionOpacity=1.f;lastAnimationChangeTime=millis();
    }
  } else if (!strcmp(commandName,"resetUnreadableLibrary")) {
    success=resetUnreadableLibrary(doc["value"],message);
  } else if (!strcmp(commandName, "setColorCalibration")) {
    JsonObject value=doc["value"].as<JsonObject>();
    success=value["r"].is<int>() && value["g"].is<int>() && value["b"].is<int>() &&
      value["gamma100"].is<int>() && value["photoCorrection"].is<bool>();
    int r=value["r"],g=value["g"],b=value["b"],gamma=value["gamma100"];
    success=success && r>=0 && r<=100 && g>=0 && g<=100 && b>=0 && b<=100 && gamma>=100 && gamma<=260;
#if !defined(MATRIX_PROFILE_WS2812)
    if(success)success=displayBackend.setChannelBalance(r,g,b);
#else
    success=success && r==100 && g==100 && b==100;
#endif
    if(success){panelBalance[0]=r;panelBalance[1]=g;panelBalance[2]=b;photoGamma100=gamma;
      photoCorrectionEnabled=value["photoCorrection"];rebuildPhotoTransfer();
      importedRgbSignature=0;huePaletteAnimation=-1;calibrationStarted=millis();}
    message=success?"Calibration applied (not saved)":"Invalid calibration or insufficient memory";
  } else if (!strcmp(commandName,"saveColorCalibration")) {
    success=saveColorCalibration();message=success?"Device calibration saved":"Could not save calibration";
  } else if (!strcmp(commandName,"setCalibrationMode")) {
    int mode=doc["value"].as<int>();success=doc["value"].is<int>() && mode>=0 && mode<=3;
#if defined(MATRIX_PROFILE_WS2812)
    success=success && mode==0;
#elif MATRIX_COLOR_DEPTH < 4
    success=success && mode!=3;
#endif
    if(success){calibrationMode=mode;depthComparisonEnabled=false;luaHelloEnabled=false;
      calibrationStarted=millis();lastAnimationChangeTime=millis();
      autoCycleTransitionPhase=0;transitionOpacity=1.f;}
    message=success?"Calibration view changed (10 minute timeout)":"Comparison needs a 4-bit HUB75 build";
  } else if (!strcmp(commandName,"setCalibrationPhoto")) {
    int id=doc["value"].as<int>();success=doc["value"].is<int>() && id>=0 && id<2;
    if(success){calibrationPhoto=id;calibrationStarted=millis();}
  } else if (!strcmp(commandName,"setCalibrationLevel")) {
    int level=doc["value"].as<int>();success=doc["value"].is<int>() && level>=32 && level<=128;
    if(success){calibrationLevel=level;calibrationStarted=millis();}
  } else if (!strcmp(commandName, "setPatternQuality")) {
    const int quality=doc["value"].as<int>();
    if(currentAnimation!=PATTERNFLOW_COLLECTION || quality<0 || quality>1)success=false;
    else{
      const uint8_t previous=patternflowQuality[pfcParams.variant];
      patternflowQuality[pfcParams.variant]=quality;
      Preferences qualityStore;
      uint64_t bits=0;
      const int first=(pfcParams.variant/64)*64;
      const char* qualityKey=first?"qbits_hi":"qbits";
      for(int i=first;i<min(first+64,int(PATTERNFLOW_PATTERN_COUNT));i++)bits|=uint64_t(patternflowQuality[i]!=0)<<(i-first);
      const bool opened=qualityStore.begin("matrix-cfg",false);
      const size_t written=opened ? qualityStore.putULong64(qualityKey,bits) : 0;
      success=written==sizeof(bits) && qualityStore.getULong64(qualityKey,~bits)==bits;
      qualityStore.end();
      if(success){
        patternflowPack.reset(pfcParams.variant);
        message=quality==0 ? "Native pixel detail saved" : "Smooth fast rendering saved";
      }else{
        patternflowQuality[pfcParams.variant]=previous;
        message=String("Could not save quality (storage opened: ")+(opened?"yes":"no")+", bytes: "+written+")";
      }
    }
  } else if (!strcmp(commandName, "setBookmark")) {
    const int index = doc["value"]["index"].as<int>();
    if (index < 0 || index >= GALLERY_PATTERN_COUNT || !doc["value"]["enabled"].is<bool>()) success = false;
    else {
      const bool previous=patternBookmarks[index];
      patternBookmarks[index] = doc["value"]["enabled"].as<bool>();
      uint64_t bits=0;
      for(int i=0;i<GALLERY_PATTERN_COUNT;i++)bits|=uint64_t(patternBookmarks[i])<<bookmarkStorageIndex(i);
      Preferences flags;
      success=flags.begin("matrix-cfg",false) && flags.putULong64("bmarks",bits)==sizeof(bits) &&
        flags.getULong64("bmarks",~bits)==bits;
      flags.end();
      if(!success){patternBookmarks[index]=previous;message="Could not save bookmark to device storage";}
    }
  } else if (!strcmp(commandName, "setPattern")) {
    const int value = doc["value"].as<int>();
    if (!isAvailableEngine(value)||value==PATTERNFLOW_COLLECTION||isPatternArchived(value,0)) {success=false;message="Restore this pattern from the archive before playing";}
    else {
      luaHelloEnabled = false;
      depthComparisonEnabled = false;calibrationMode=0;
      currentAnimation = (Animation)value;
      loadPatch(currentAnimation, defaultPatches[currentAnimation]);
      autoCycleTransitionPhase = 0;
      transitionOpacity = 1.0f;
      Serial.print("Pattern selected: ");
      Serial.println(animationNames[currentAnimation]);
      lastAnimationChangeTime = millis();
      message = String("Pattern: ") + animationNames[currentAnimation];
    }
  } else if (!strcmp(commandName, "setPatternVariant")) {
    const int variant = doc["value"].as<int>();
    if (!isAvailableVariant(variant)||isPatternArchived(PATTERNFLOW_COLLECTION,variant)) {success=false;message="Restore this pattern from the archive before playing";}
    else {
      luaHelloEnabled = false;
      depthComparisonEnabled = false;calibrationMode=0;
      currentAnimation = PATTERNFLOW_COLLECTION;
      loadVariantPatch(variant,variantDefaults[variant]);
      const PatternflowMetadata& metadata=PATTERNFLOW_METADATA[variant];
      autoCycleTransitionPhase = 0;
      transitionOpacity = 1.0f;
      lastAnimationChangeTime = millis();
      message = String("Pattern: ") + metadata.name;
    }
  } else if (!strcmp(commandName, "setAutoCycle")) {
    autoCycleEnabled = doc["value"].as<bool>();
    preferences.begin("matrix-cfg", false); preferences.putBool("auto_cycle", autoCycleEnabled); preferences.end();
    lastAnimationChangeTime = millis();
    message = autoCycleEnabled ? "Auto-cycle enabled" : "Auto-cycle disabled";
  } else if (!strcmp(commandName, "setFPS")) {
    targetFPS = clampInt(doc["value"].as<int>(), 1, 240);
    preferences.begin("matrix-cfg", false); preferences.putInt("global_fps", targetFPS); preferences.end();
    message = "Target FPS saved";
  } else if (!strcmp(commandName, "setCycleInterval")) {
    autoCycleIntervalSecs = constrain(doc["value"].as<unsigned long>(), 1UL, 86400UL);
    preferences.begin("matrix-cfg", false); preferences.putULong("cycle_int", autoCycleIntervalSecs); preferences.end();
    lastAnimationChangeTime = millis();
    message = "Cycle interval saved";
  } else if (!strcmp(commandName, "setSchedule")) {
    scheduleEnabled = doc["enabled"].as<bool>();
    scheduleStartMinutes = constrain(doc["startMinutes"].as<int>(), 0, 1439);
    scheduleEndMinutes = constrain(doc["endMinutes"].as<int>(), 0, 1439);
    scheduleFadeInSeconds = constrain(doc["fadeInSeconds"].as<int>(), 1, 3600);
    scheduleFadeOutSeconds = constrain(doc["fadeOutSeconds"].as<int>(), 1, 3600);
    preferences.begin("matrix-cfg", false);
    preferences.putBool("sched_on", scheduleEnabled);
    preferences.putUShort("sched_start", scheduleStartMinutes);
    preferences.putUShort("sched_end", scheduleEndMinutes);
    preferences.putUShort("sched_fadein", scheduleFadeInSeconds);
    preferences.putUShort("sched_fadeout", scheduleFadeOutSeconds);
    preferences.end();
    message = "Time schedule saved";
  } else if (!strcmp(commandName, "setResyncInterval")) {
    autoResyncInterval = constrain(doc["value"].as<unsigned long>(), 0UL, 86400UL);
    preferences.begin("matrix-cfg", false); preferences.putULong("resync_int", autoResyncInterval); preferences.end();
    lastResyncTime = millis();
    message = "Legacy re-sync interval saved";
  } else if (!strcmp(commandName, "setParam")) {
    const int animIndex = doc["animIndex"].as<int>();
    const char* paramName = doc["paramName"];
    success = setAnimationParam(animIndex, paramName, doc["value"]);
    message = success ? "Parameter updated" : "Unknown parameter";
  } else if (!strcmp(commandName, "savePatch")) {
    const int animIndex = doc["animIndex"].as<int>();
    const int patchIndex = doc["patchIndex"].as<int>();
    if (!isAvailableEngine(animIndex) || patchIndex < 0 || patchIndex >= NUM_PATCH_SLOTS || animIndex != currentAnimation) success = false;
    else {
      if(animIndex==PATTERNFLOW_COLLECTION)success=saveVariantPatch(patchIndex);else savePatchToPreferences(animIndex, patchIndex);
      if(success&&patchIndex==defaultPatches[animIndex])success=saveSoukanOverride(animIndex,pfcParams.variant);
      message = success?String("Saved patch ") + (patchIndex + 1):String("Could not save patch to device storage");
    }
  } else if (!strcmp(commandName, "loadPatch")) {
    const int animIndex = doc["animIndex"].as<int>();
    const int patchIndex = doc["patchIndex"].as<int>();
    if (!isAvailableEngine(animIndex) || isPatternArchived(animIndex,pfcParams.variant) || patchIndex < 0 || patchIndex >= NUM_PATCH_SLOTS) success = false;
    else {
      luaHelloEnabled = false;
      depthComparisonEnabled = false;calibrationMode=0;
      currentAnimation = (Animation)animIndex;
      loadPatch(currentAnimation, patchIndex);
      autoCycleTransitionPhase = 0;
      transitionOpacity = 1.0f;
      lastAnimationChangeTime = millis();
      message = String("Loaded patch ") + (patchIndex + 1);
    }
  } else if (!strcmp(commandName, "setDefault")) {
    const int animIndex = doc["animIndex"].as<int>();
    const int patchIndex = doc["patchIndex"].as<int>();
    if (!isAvailableEngine(animIndex) || patchIndex < 0 || patchIndex >= NUM_PATCH_SLOTS) success = false;
    else {
      defaultPatches[animIndex] = patchIndex;
      if(animIndex==PATTERNFLOW_COLLECTION){const uint8_t previous=variantDefaults[pfcParams.variant];variantDefaults[pfcParams.variant]=patchIndex;success=storeVariantPatches();if(!success){variantDefaults[pfcParams.variant]=previous;defaultPatches[animIndex]=previous;}}
      preferences.begin("matrix-cfg", false); preferences.putBytes("def_p", defaultPatches, sizeof(defaultPatches)); preferences.end();
      if(success)success=saveSoukanOverride(animIndex,pfcParams.variant);
      message = success?String("Patch ") + (patchIndex + 1) + " is the auto-cycle default":String("Could not save default to device storage");
    }
  } else if (!strcmp(commandName, "saveCurrentDefault")) {
    const int animIndex = (int)currentAnimation;
    const int patchIndex = defaultPatches[animIndex];
    if(animIndex==PATTERNFLOW_COLLECTION)success=saveVariantPatch(patchIndex);else savePatchToPreferences(animIndex, patchIndex);
    if(success)success=saveSoukanOverride(animIndex,pfcParams.variant);
    message = success?String(animationNames[animIndex]) +
      " parameters saved permanently to default patch " + (patchIndex + 1):String("Could not save default to device storage");
  } else if (!strcmp(commandName, "runLuaHello")) {
    if (!luaPattern.available()) {
      success = false;
      message = String("Lua unavailable: ") + luaPattern.lastError();
    } else {
      depthComparisonEnabled = false;calibrationMode=0;
      luaHelloEnabled = true;
      animationTime = 0;
      lastAnimationChangeTime = millis();
      message = "Hello Lua started";
    }
  } else if (!strcmp(commandName, "stopLuaHello")) {
    luaHelloEnabled = false;
    lastAnimationChangeTime = millis();
    message = "Hello Lua stopped";
  } else if (!strcmp(commandName, "setDepthComparison")) {
#if defined(MATRIX_PROFILE_WS2812) || MATRIX_COLOR_DEPTH < 4
    success = false;
    message = "True 3/4-bit comparison needs a 4-bit HUB75 build";
#else
    calibrationMode=0;
    depthComparisonEnabled = doc["value"].as<bool>();
    if (depthComparisonEnabled) {
      luaHelloEnabled = false;
      animationTime = 0;
    }
    lastAnimationChangeTime = millis();
    message = depthComparisonEnabled
      ? "3-bit / 4-bit comparison started"
      : "3-bit / 4-bit comparison stopped";
#endif
  } else if (!strcmp(commandName, "resync")) {
    resyncDisplay();
    message = "Frame timing refreshed";
  } else {
    success = false;
    message = "Unknown command";
  }

  if (success) stateRevision++;
  isUpdatingWeb = false;
  status = success ? 200 : 400;
  return makeCommandResultJson(success, message);
}

void handleControlPost() {
  int status = 500;
  const String response = processControlCommandJson(server.arg("plain"), status);
  server.send(status, "application/json", response);
}

String buildWifiScanJson() {
  const int found = WiFi.scanNetworks(false, true);
  DynamicJsonDocument doc(4096);
  doc["success"] = found >= 0;
  JsonArray networks = doc.createNestedArray("networks");

  if (found > 0) {
    for (int i = 0; i < found; i++) {
      const String ssid = WiFi.SSID(i);
      bool duplicate = false;
      for (JsonObject existing : networks) {
        if (existing["ssid"].as<String>() == ssid) { duplicate = true; break; }
      }
      if (duplicate || ssid.length() == 0) continue;
      JsonObject n = networks.createNestedObject();
      n["ssid"] = ssid;
      n["rssi"] = WiFi.RSSI(i);
      n["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
  }

  WiFi.scanDelete();
  String response;
  serializeJson(doc, response);
  return response;
}

void handleWifiScan() {
  const String response = buildWifiScanJson();
  server.send(200, "application/json", response);
}

void handleWifiAdd() {
  if(wifiRestartAt){sendJson(false,"Network switch already queued. Wait for restart.",409);return;}
  String ssid;
  String password;
  bool restartAfterSave = isAPMode;

  if (server.hasArg("plain") && server.arg("plain").length()) {
    DynamicJsonDocument doc(512);
    const DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error) {
      sendJson(false, "Invalid JSON", 400);
      return;
    }
    ssid = doc["ssid"].as<String>();
    password = doc["password"].as<String>();
    if (doc.containsKey("restart")) restartAfterSave = doc["restart"].as<bool>();
  } else {
    ssid = server.arg("ssid");
    if (ssid.length() == 0) ssid = server.arg("s");
    password = server.arg("password");
    if (password.length() == 0) password = server.arg("p");
  }

  ssid.trim();
  if (!addOrUpdateNetwork(ssid, password)) {
    sendJson(false, "Could not save network. Check the SSID or remove old networks.", 400);
    return;
  }

  stateRevision++;
  sendJson(true, restartAfterSave ? "Network saved. Restarting…" : "Network saved and verified. Use Connect selected to switch.");

  if (restartAfterSave) {
    delay(800);
    ESP.restart();
  }
}

void handleSetupSave() {
  handleWifiAdd();
}

void handleWifiReset() {
  sendJson(true, "Wi-Fi networks erased. Restarting in setup mode…");
  delay(300);
  preferences.begin("wifi-list", false);
  preferences.clear();
  preferences.end();
  delay(500);
  ESP.restart();
}

void handleNotFound() {
  if (isAPMode) {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
    server.send(302, "text/plain", "Redirecting to Wi-Fi setup");
  } else {
    server.send(404, "text/plain", "Not found");
  }
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/preview-atlas.png", HTTP_GET, handlePreviewAtlas);
  server.on("/preview", HTTP_GET, handlePreviewGet);
  server.on("/setup", HTTP_GET, handleSetupPage);
  server.on("/control", HTTP_GET, handleControlGet);
  server.on("/library", HTTP_GET, [](){server.send(200,"application/json",buildLibraryJson());});
  server.on("/control", HTTP_POST, handleControlPost);
  server.on("/diagnostics", HTTP_GET, handleDiagnosticsGet);

  server.on("/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/wifi/add", HTTP_POST, handleWifiAdd);
  server.on("/wifi/reset", HTTP_POST, handleWifiReset);

  // Compatibility aliases for the routes used in the second sketch.
  server.on("/addNetwork", HTTP_POST, handleWifiAdd);
  server.on("/resetWifi", HTTP_POST, handleWifiReset);
  server.on("/save", HTTP_POST, handleSetupSave);

  // Common captive-portal probes.
  server.on("/generate_204", HTTP_ANY, handleSetupPage);
  server.on("/gen_204", HTTP_ANY, handleSetupPage);
  server.on("/hotspot-detect.html", HTTP_ANY, handleSetupPage);
  server.on("/library/test/success.html", HTTP_ANY, handleSetupPage);
  server.on("/ncsi.txt", HTTP_ANY, handleSetupPage);
  server.on("/connecttest.txt", HTTP_ANY, handleSetupPage);
  server.on("/redirect", HTTP_ANY, handleSetupPage);
  server.on("/fwlink", HTTP_ANY, handleSetupPage);

  server.onNotFound(handleNotFound);
}
