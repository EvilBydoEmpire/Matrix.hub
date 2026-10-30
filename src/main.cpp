// Shared Matrix.hub firmware; platformio.ini selects the panel.
#if defined(MATRIX_PROFILE_WS2812) == defined(MATRIX_PROFILE_HUB75)
#error "Select exactly one display profile in platformio.ini"
#endif

#if defined(MATRIX_PROFILE_WS2812)
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
#else
#include "platform/DmaMatrix.h"
#endif
#include <Adafruit_GFX.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <math.h>
#include <nvs.h>
#include <time.h>

#if !defined(MATRIX_PROFILE_WS2812)
#include "platform/Hub75Backend.h"
#endif
#include "platform/LedEncoder.h"
#include "platform/PreferenceWrites.h"
#include "platform/SerialLineBuffer.h"
#include "patterns/ArtDirection.h"
#include "patterns/FastMath.h"
#include "patterns/NativeEngines.h"
#include "patterns/Palettes.h"
#include "patterns/PatternCatalog.h"
#include "patterns/PatternStore.h"
#include "patterns/PatternflowPack.h"
#include "patterns/Rgb565.h"
#include "generated/PatternPreviewAtlas.h"
#include "generated/WebAssets.h"

// The commit the build stamps; empty when built outside Git.
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION ""
#endif

// -----------------------------------------------------------------------------
// Hardware profiles
// -----------------------------------------------------------------------------
#if defined(MATRIX_PROFILE_WS2812)
#if !defined(WS2812_MATRIX_WIDTH) || !defined(WS2812_MATRIX_HEIGHT) || !defined(WS2812_DATA_PIN) || !defined(WS2812_ROTATION)
#error "platformio.ini sets the WS2812 size, data pin and rotation"
#endif
#define SCREEN_WIDTH    WS2812_MATRIX_WIDTH
#define SCREEN_HEIGHT   WS2812_MATRIX_HEIGHT

#if WS2812_MATRIX_WIDTH == 22 && WS2812_MATRIX_HEIGHT == 22
const char* DEVICE_PROFILE_ID = "esp32-wroom-ws2812-22x22";
const char* DEVICE_DISPLAY_NAME = "Matrix.22";
const char* HOSTNAME = "matrix-22";
const char* SETUP_AP_SSID = "Matrix.22-Setup";
#elif WS2812_MATRIX_WIDTH == 16 && WS2812_MATRIX_HEIGHT == 16
const char* DEVICE_PROFILE_ID = "esp32-wroom-ws2812-16x16";
const char* DEVICE_DISPLAY_NAME = "Matrix.16";
const char* HOSTNAME = "matrix-16";
const char* SETUP_AP_SSID = "Matrix.16-Setup";
#else
#error "WS2812 panels are 16×16 or 22×22"
#endif
const char* DISPLAY_BACKEND_NAME = "ws2812";

// Data enters at the top-left, rows run horizontally, and alternate rows
// reverse direction.
class PlatformNeoMatrix : public Adafruit_NeoMatrix {
 public:
  using Adafruit_NeoMatrix::Adafruit_NeoMatrix;
  uint16_t color565(uint8_t red, uint8_t green, uint8_t blue) const {
    return Color(red, green, blue);
  }
  // Stores 0xRRGGBB in an LED's bytes, in the strip's colour order. NeoPixel
  // brightness stays at 255, so nothing scales it. Like setPixelColor, it skips
  // LEDs the strip lacks, all of them when its buffer could not be allocated.
  void setLed(uint16_t index, uint32_t color) {
    if (index >= numLEDs) return;
    uint8_t* led = pixels + 3 * index;
    led[rOffset] = color >> 16;
    led[gOffset] = color >> 8;
    led[bOffset] = color;
  }
};

PlatformNeoMatrix matrix(
  SCREEN_WIDTH, SCREEN_HEIGHT, WS2812_DATA_PIN,
  NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG,
  NEO_GRB + NEO_KHZ800
);

// The LED behind each panel pixel, learned from NeoMatrix at boot so its layout
// and rotation stay the one description of the wiring.
uint16_t ledIndex[SCREEN_HEIGHT][SCREEN_WIDTH];

void learnLedLayout() {
  const uint8_t* leds = matrix.getPixels();
  if (!leds) return;
  matrix.setPassThruColor(0xFFFFFF);
  for (int y = 0; y < SCREEN_HEIGHT; y++) for (int x = 0; x < SCREEN_WIDTH; x++) {
    matrix.clear();
    matrix.drawPixel(x, y, 0);
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++)
      if (leds[3 * i]) { ledIndex[y][x] = i; break; }
  }
  matrix.setPassThruColor();
  matrix.clear();
}

#else
#define SCREEN_WIDTH    64
#define SCREEN_HEIGHT   64

const char* DEVICE_PROFILE_ID = "matrixportal-s3-hub75-64x64";
const char* DEVICE_DISPLAY_NAME = "Matrix.64";
const char* DISPLAY_BACKEND_NAME = "hub75-dma";
const char* HOSTNAME = "matrix-64";
const char* SETUP_AP_SSID = "Matrix.64-Setup";

DmaMatrix matrix;
Hub75Backend displayBackend(matrix);
#endif

// Sends the finished frame to the panel.
void showFrame() {
#if defined(MATRIX_PROFILE_WS2812)
  matrix.show();
#else
  displayBackend.show();
#endif
}

// -----------------------------------------------------------------------------
// Wi-Fi configuration
// -----------------------------------------------------------------------------
const byte DNS_PORT = 53;
const uint8_t MAX_WIFI_NETWORKS = 8;
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 7000;
const unsigned long WIFI_LOST_TO_AP_MS = 60000;
const unsigned long WIFI_RETRY_INTERVAL_MS = 60000;

// -----------------------------------------------------------------------------
// Matrix, web server and global state
// -----------------------------------------------------------------------------
Preferences preferences;
WebServer server(80);
DNSServer dnsServer;

bool isAPMode = false;
bool mdnsStarted = false;
bool otaStarted = false;
unsigned long wifiDisconnectedSince = 0;
unsigned long wifiRestartAt = 0;
// Saved networks are tried in rounds: whether the boot round is underway, the
// attempts made in this round (0 between rounds), the slot tried last and when
// that began.
bool wifiJoining = false;
int wifiRetryPosition = 0;
int wifiRetrySlot = -1;
unsigned long wifiRetryAt = 0;

constexpr unsigned long FRAME_DURATION_US = 1000000UL / 60;
bool autoCycleEnabled = true;
unsigned long autoCycleIntervalSecs = 300;

bool scheduleEnabled = false;
uint16_t scheduleStartMinutes = 18 * 60;
uint16_t scheduleEndMinutes = 0;
uint16_t scheduleFadeInSeconds = 30;
uint16_t scheduleFadeOutSeconds = 30;
float scheduleOpacity = 1.0f;
float transitionOpacity = 1.0f;
float combinedOutputScale = 1.0f;
#if defined(MATRIX_PROFILE_WS2812)
LedEncoder ledEncoder;
uint32_t ledOutputGain = 0;
#endif
uint8_t transitionPhase = 0; // 0 idle, 1 fading out, 2 fading in
unsigned long transitionStartedMs = 0;
bool transitionAdvances = false;  // Cycling to the next pattern, or renewing this one.
// Pattern clocks are floats, so a pattern that has played for an hour restarts
// behind the transition before rounding can make its motion stutter.
constexpr unsigned long PATTERN_RENEWAL_MS = 3600000UL;
unsigned long patternStartedMs = 0;
constexpr uint16_t TRANSITION_FADE_OUT_MS = 900;
constexpr uint16_t TRANSITION_FADE_IN_MS = 1600;
unsigned long lastFadeUpdateMs = 0;

const int NUM_ANIMATIONS = PATTERNFLOW_COLLECTION + 1;
const char* animationNames[NUM_ANIMATIONS] = {
  "Starfield", "Warped Checkerboard", nullptr, "Moiré Lines", nullptr,
  "Shockwave", "Warp Corridors", "Pixel Avalanche", nullptr, "Bayer Flow", nullptr,
  "Snake", "Squares", nullptr, "Flow Sphere", nullptr, nullptr
};

Animation currentAnimation = STARFIELD;
// One device-wide brightness, 0..15; low by default.
uint8_t deviceBrightness = 3;
unsigned long lastAnimationChangeTime = 0;
struct PatternflowCollectionParams { uint8_t variant, control[4]; };
// Saturation scales the palette's colourfulness, 100 as designed.
struct ColorParams              { uint16_t saturation; uint8_t palette; };
StarfieldParams sfParams;
WarpedCheckerboardParams wcParams;
MoireLinesParams mlParams;
ShockwaveParams swParams;
WarpCorridorsParams wcoParams;
PixelAvalancheParams paParams;
BayerFlowParams bdParams;
SnakeParams snParams;
SquaresParams sqParams;
FlowSphereParams fsParams;
PatternflowCollectionParams pfcParams;
PatternflowPack patternflowPack;
NativeEngines nativeEngines;
ColorParams colorParams[NUM_ANIMATIONS];
// The four colours of each configuration's palette; choosing a palette loads its stops.
uint32_t paletteStops[NUM_ANIMATIONS][4];

// The playing pattern's palette, sampled at 256 positions around its loop, so
// full-screen effects look colours up instead of computing them per pixel.
uint16_t paletteTable[256];
int paletteTableBrightness = -1;
int paletteTableAnimation = -1;
ColorParams paletteTableParams = {0, 255};

unsigned long lastFrameTime = 0;
unsigned long nextFrameDueTime = 0;

struct PerformanceMetrics {
  float actualFPS = 0.0f;
  float panelRefreshHz = 0.0f;
  uint32_t averageRenderUs = 0;
  uint32_t averageDrawUs = 0;
  uint32_t averageShowUs = 0;
  uint32_t worstRenderUs = 0;
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

// Fits the longest app command, including JSON-escaped Wi-Fi credentials.
using UsbSerialBuffer = SerialLineBuffer<1024>;
UsbSerialBuffer usbSerial;

// Defined further down, used earlier.
void setupWebServer();
void loadSettings();
void resumeLastPattern();
void playPattern(int engine, int variant);
void restartPattern();
void flushConfig();
void persistSettledSettings(bool immediately = false);
void setMatrixPointColor(int y, int x, uint16_t color);
void drawPatternPixel(void*, int16_t x, int16_t y, uint16_t color);
float colorScale(int brightness);
uint16_t makeColor(float hue, float saturation, float value, int brightness);
void ensurePaletteTable(int brightness);
uint16_t makePaletteColor(float position, int brightness, const ColorParams& controls);
size_t engineControls(int engine, void*& controls);
void startWifi();
void startAccessPoint();
void maintainWifi();
String buildControlJson();
String buildDiagnosticsJson();
String processControlCommandJson(const String& body, int& status);

#include "patterns/ColorCalibration.inc"
#include "patterns/PatternColors.inc"
#include "patterns/Library.inc"

// Restarts after saving edits that are still waiting to settle.
void restartDevice() {
  persistSettledSettings(true);
  ESP.restart();
}

bool forgetWifiNetworks() {
  stateRevision++;
  return clearPreferencesNamespace(preferences, "wifi-list");
}

// The USB bridge's protocol: one request per line, one answer per line.
void parseUsbSerialLine(const char* line) {
  if (!strcmp(line, "USBGET,CONTROL")) {
    Serial.print("USBJSON,CONTROL,");
    Serial.println(buildControlJson());
  } else if (!strcmp(line, "USBGET,DIAGNOSTICS")) {
    Serial.print("USBJSON,DIAGNOSTICS,");
    Serial.println(buildDiagnosticsJson());
  } else if (!strncmp(line, "USBPOST,CONTROL,", 16)) {
    int status = 500;
    const String result = processControlCommandJson(String(line + 16), status);
    Serial.printf("USBJSON,RESULT,%d,%s\n", status, result.c_str());
  } else if (!strcmp(line, "WIFI,RESET")) {
    if (!forgetWifiNetworks()) { Serial.println("WIFI,RESET,failed"); return; }
    Serial.println("WIFI,RESET,restarting");
    Serial.flush();
    delay(150);
    restartDevice();
  }
}

void processUsbSerial() {
  while (Serial.available() > 0) {
    switch (usbSerial.feed((char)Serial.read())) {
      case UsbSerialBuffer::Line:
        parseUsbSerialLine(usbSerial.line());
        break;
      case UsbSerialBuffer::Overflow:
        // Answer in the caller's protocol so the bridge fails fast, not by timeout.
        if (!strncmp(usbSerial.line(), "USBPOST,CONTROL,", 16))
          Serial.println("USBJSON,RESULT,413,{\"success\":false,\"message\":\"Command too long\"}");
        else
          Serial.println("USB,ERROR,line too long");
        break;
      case UsbSerialBuffer::Pending:
        break;
    }
  }
}

void renderInstallingUpdateFrame() {
  matrix.fillScreen(0);
#if defined(MATRIX_PROFILE_WS2812)
  const uint16_t border = matrix.color565(32, 16, 0);
  const uint16_t activity = matrix.color565(0, 32, 32);
  matrix.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, border);
  // A two-row shaft on even panels keeps the arrow centred inside the border.
  const int top = (SCREEN_HEIGHT - 1) / 2, bottom = SCREEN_HEIGHT / 2;
  const int tip = SCREEN_WIDTH - 4;
  for (int y = top; y <= bottom; y++) matrix.drawLine(3, y, tip, y, activity);
  matrix.drawLine(tip - 3, top - 3, tip, top, activity);
  matrix.drawLine(tip - 3, bottom + 3, tip, bottom, activity);
#else
  matrix.setTextWrap(false);
  matrix.setTextSize(1);
  matrix.setTextColor(matrix.color565(0, 32, 32));
  matrix.setCursor(2, 25);
  matrix.print("INSTALLING");
  matrix.setCursor(11, 36);
  matrix.print("UPDATE");
#endif
  showFrame();
}

void setupOta() {
  if (otaStarted || WiFi.status() != WL_CONNECTED) return;
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.onStart([]() {
    persistSettledSettings(true);  // Keep pending edits across the update.
    renderInstallingUpdateFrame();
#if !defined(MATRIX_PROFILE_WS2812)
    // Flash writes stall interrupts and can freeze a lit HUB75 row: show the
    // notice briefly, then stop the panel output for the whole transfer.
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
    Serial.printf("OTA update failed: %u\n", (unsigned)error);
  });
  ArduinoOTA.begin();
  otaStarted = true;
  Serial.println("Arduino OTA receiver ready");
}

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------
void setup() {
#if !ARDUINO_USB_CDC_ON_BOOT
  // Replies wait in a buffer for the UART instead of holding frames back.
  Serial.setTxBufferSize(4096);
#endif
  Serial.begin(115200);
  delay(100);
  Serial.printf("\n\nMatrix.hub on %s\n", DEVICE_DISPLAY_NAME);

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
  learnLedLayout();
  ledEncoder.begin();
  Serial.printf("WS2812 ready: %dx%d, GPIO %d, rotation %d\n",
                SCREEN_WIDTH, SCREEN_HEIGHT, WS2812_DATA_PIN,
                WS2812_ROTATION);
#else
  // Without a panel the matrix stays reachable, so an update can still fix it.
  if (matrix.begin()) Serial.println("HUB75 ready: 64x64, 4-bit DMA at 20 MHz");
  else Serial.println("HUB75 output could not start");
#endif
  matrix.fillScreen(0);
  showFrame();

  loadColorCalibration();  // Before the settings, so a reset keeps the white balance.
  loadSettings();
  resumeLastPattern();

  ensureFastMath();

  setupWebServer();
  startWifi();
  server.begin();
  Serial.println("HTTP server started");

  lastAnimationChangeTime = millis();
  lastFrameTime = micros();
  nextFrameDueTime = lastFrameTime;
}

// -----------------------------------------------------------------------------
// Main loop
// -----------------------------------------------------------------------------
// What the panel shows, the test chart included.
const char* playingName() {
  if (testChartShown) return "Test chart";
  return currentAnimation == PATTERNFLOW_COLLECTION
    ? PATTERNFLOW_METADATA[constrain(pfcParams.variant, 0, PATTERNFLOW_PATTERN_COUNT - 1)].name
    : animationNames[(int)currentAnimation];
}

void recordRenderedFrame(uint32_t drawUs, uint32_t showUs,
                         uint32_t frameBudgetUs) {
  const unsigned long nowMs = millis();
  if (performance.windowStartedAtMs == 0) {
    performance.windowStartedAtMs = nowMs;
#if !defined(MATRIX_PROFILE_WS2812)
    matrix.getFrameCount(); // Exclude scans from startup and the network connection.
#endif
  }

  const uint32_t renderUs = drawUs + showUs;
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
    Serial.printf(
      "PERF pattern=%s fps=%.1f panel_refresh=%.1fHz draw=%.2fms show=%.2fms render_avg=%.2fms render_worst=%.2fms over_budget=%lu/%lu heap=%lu min_heap=%lu\n",
      playingName(), performance.actualFPS, performance.panelRefreshHz,
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

// Minutes since local midnight, or -1 until NTP has set the clock.
int localMinute() {
  const time_t now = time(nullptr);
  if (now < 1700000000) return -1;  // The clock counts from 1970 until it syncs.
  struct tm local;
  localtime_r(&now, &local);
  return local.tm_hour * 60 + local.tm_min;
}

bool scheduleShouldBeOn() {
  if (!scheduleEnabled || scheduleStartMinutes == scheduleEndMinutes) return true;
  const int minute = localMinute();
  if (minute < 0) return true;
  if (scheduleStartMinutes < scheduleEndMinutes) {
    return minute >= scheduleStartMinutes && minute < scheduleEndMinutes;
  }
  return minute >= scheduleStartMinutes || minute < scheduleEndMinutes;
}

void advanceAutoCyclePattern() {
  flushConfig();
  const int next=nextCycledIndex(galleryIndex(currentAnimation,pfcParams.variant));
  if(next<CORE_PATTERN_COUNT)playPattern(CORE_PATTERN_IDS[next],0);
  else playPattern(PATTERNFLOW_COLLECTION,COLLECTION_PATTERN_IDS[next-CORE_PATTERN_COUNT]);
}

void updatePlaybackFades() {
  const unsigned long now = millis();
  const float elapsedSeconds = lastFadeUpdateMs == 0
    ? 0.0f
    : min((now - lastFadeUpdateMs) / 1000.0f, 0.25f);
  lastFadeUpdateMs = now;

  const bool shouldBeOn = scheduleShouldBeOn();
  // The first synced reading sets the level at once: a restart inside the off
  // window goes dark instead of fading out from full brightness.
  static bool clockSynced = false;
  if (!clockSynced && localMinute() >= 0) {
    clockSynced = true;
    scheduleOpacity = shouldBeOn ? 1.0f : 0.0f;
  }
  const float duration = max(0.1f, (shouldBeOn
    ? scheduleFadeInSeconds
    : scheduleFadeOutSeconds) * 1.0f);
  const float step = elapsedSeconds / duration;
  scheduleOpacity = constrain(scheduleOpacity + (shouldBeOn ? step : -step),
                              0.0f, 1.0f);

  const bool cycleDue = autoCycleEnabled &&
    now - lastAnimationChangeTime >= autoCycleIntervalSecs * 1000UL;
  if (!testChartShown && transitionPhase == 0 &&
      (cycleDue || now - patternStartedMs >= PATTERN_RENEWAL_MS)) {
    transitionPhase = 1;
    transitionStartedMs = now;
    transitionAdvances = cycleDue;
  }

  if (transitionPhase == 1) {
    const float progress = constrain(
      (now - transitionStartedMs) / (float)TRANSITION_FADE_OUT_MS,
      0.0f, 1.0f);
    transitionOpacity = 1.0f - progress;
    if (progress >= 1.0f) {
      if (transitionAdvances) {
        advanceAutoCyclePattern();
        lastAnimationChangeTime = now;
        stateRevision++;
      } else {
        restartPattern();
      }
      transitionPhase = 2;
      transitionStartedMs = now;
    }
  } else if (transitionPhase == 2) {
    const float progress = constrain(
      (now - transitionStartedMs) / (float)TRANSITION_FADE_IN_MS,
      0.0f, 1.0f);
    transitionOpacity = progress;
    if (progress >= 1.0f) {
      transitionOpacity = 1.0f;
      transitionPhase = 0;
    }
  } else {
    transitionOpacity = 1.0f;
  }

  combinedOutputScale = testChartShown ? 1.0f : scheduleOpacity * transitionOpacity;
}

void loop() {
  processUsbSerial();
  if (otaStarted) ArduinoOTA.handle();
  server.handleClient();
  if (wifiRestartAt && int32_t(millis()-wifiRestartAt)>=0) restartDevice();
  if (isAPMode) dnsServer.processNextRequest();
  maintainWifi();
  persistSettledSettings();
  if (testChartShown && millis() - testChartShownAt > TEST_CHART_TIMEOUT_MS) {
    testChartShown = false;
    lastAnimationChangeTime = millis();
    stateRevision++;
  }
  updatePlaybackFades();

  const unsigned long currentTime = micros();
  if ((int32_t)(currentTime - nextFrameDueTime) < 0) return;

  // Keep a stable deadline instead of restarting the timer from a slightly
  // late frame. This prevents small Wi-Fi/web scheduling delays from lowering
  // the average FPS. Skip stale deadlines after a larger interruption.
  if ((unsigned long)(currentTime - nextFrameDueTime) > FRAME_DURATION_US) {
    nextFrameDueTime = currentTime;
  }
  nextFrameDueTime += FRAME_DURATION_US;

  float deltaTime = (currentTime - lastFrameTime) / 1000000.0f;
  if (deltaTime > 0.12f) deltaTime = 0.12f; // avoid giant simulation jumps
  lastFrameTime = currentTime;
  const unsigned long renderStartedAt = micros();
#if defined(MATRIX_PROFILE_WS2812)
  ledOutputGain = LedEncoder::gain(deviceBrightness, combinedOutputScale);
#endif

  if (testChartShown) {
    renderTestChart();
    const unsigned long showStartedAt = micros();
    showFrame();
    recordRenderedFrame(showStartedAt - renderStartedAt,
                        micros() - showStartedAt, FRAME_DURATION_US);
    return;
  }

  matrix.fillScreen(0);
  ensurePaletteTable(deviceBrightness);
  if (currentAnimation != PATTERNFLOW_COLLECTION) {
    void* controls;
    engineControls(currentAnimation, controls);
    nativeEngines.render(currentAnimation, SCREEN_WIDTH, SCREEN_HEIGHT, deltaTime, controls, paletteTable,
                         colorScale(deviceBrightness), drawPatternPixel, nullptr);
  } else {
    patternflowPack.render(pfcParams.variant, SCREEN_WIDTH, SCREEN_HEIGHT, deltaTime, pfcParams.control,
                           paletteTable, collectionSourceColors(deviceBrightness), drawPatternPixel, nullptr);
  }

  const unsigned long showStartedAt = micros();
  showFrame();
  recordRenderedFrame(showStartedAt - renderStartedAt,
                      micros() - showStartedAt, FRAME_DURATION_US);
}

// -----------------------------------------------------------------------------
// Settings and pattern configurations
// -----------------------------------------------------------------------------
// Compiled defaults for one pattern: its controls and palette.
void applyDefaults(int engine, int variant) {
  if (engine == PATTERNFLOW_COLLECTION) {
    pfcParams = {uint8_t(variant), {0, 0, 0, 0}};
    memcpy(pfcParams.control, PATTERNFLOW_METADATA[variant].defaults, 4);
  } else {
    void* controls;
    if (engineControls(engine, controls)) nativeDefaults(engine, controls);
  }
  colorParams[engine] = {100, Palettes::forPattern(engine, variant)};
  compileStops(paletteStops[engine], engine, variant, colorParams[engine].palette);
}

// An engine's live controls, as its stored record holds them.
size_t engineControls(int engine, void*& controls) {
  switch (engine) {
    case STARFIELD:              controls = &sfParams;  return sizeof(sfParams);
    case WARPED_CHECKERBOARD:    controls = &wcParams;  return sizeof(wcParams);
    case MOIRE_LINES:            controls = &mlParams;  return sizeof(mlParams);
    case SHOCKWAVE:              controls = &swParams;  return sizeof(swParams);
    case WARP_CORRIDORS:         controls = &wcoParams; return sizeof(wcoParams);
    case PIXEL_AVALANCHE:        controls = &paParams;  return sizeof(paParams);
    case BAYER_FLOW:             controls = &bdParams;  return sizeof(bdParams);
    case SNAKE:                  controls = &snParams;  return sizeof(snParams);
    case SQUARES:                controls = &sqParams;  return sizeof(sqParams);
    case FLOW_SPHERE:            controls = &fsParams;  return sizeof(fsParams);
    case PATTERNFLOW_COLLECTION: controls = &pfcParams; return sizeof(pfcParams);
    default:                     controls = nullptr;    return 0;
  }
}

// A stored pattern record: its look, then its engine's controls.
struct PatternLook {
  ColorParams color;
  uint32_t stops[4];
};
constexpr size_t PATTERN_RECORD_CAPACITY = sizeof(PatternLook) + 32;

int currentPatternId() { return patternId(currentAnimation, pfcParams.variant); }

// Edits that could not be saved before another pattern started.
PatternStore::PendingRecords<PATTERN_ID_COUNT, PATTERN_RECORD_CAPACITY> pendingRecords;
bool configDirty = false;
unsigned long configSaveDueMs = 0;

bool readPatternRecord(int engine, int variant) {
  void* controls;
  const size_t size = sizeof(PatternLook) + engineControls(engine, controls);
  uint8_t record[PATTERN_RECORD_CAPACITY];
  const int id = patternId(engine, variant);
  // Unsaved edits come back with their pattern and are saved as its own.
  bool found = controls && pendingRecords.take(id, record, size);
  if (found) {
    configDirty = true;
  } else {
    Preferences store;
    found = controls && size <= sizeof(record) && store.begin("matrix-cfg", true) &&
            PatternStore::read(store, id, record, size);
    store.end();
  }
  if (!found) return false;
  PatternLook look;
  memcpy(&look, record, sizeof(look));
  colorParams[engine] = look.color;
  memcpy(paletteStops[engine], look.stops, sizeof(look.stops));
  memcpy(controls, record + sizeof(look), size - sizeof(look));
  if (engine == PATTERNFLOW_COLLECTION) {
    pfcParams.variant = variant;
    // A control hidden after the record was saved returns to its default.
    for (int i = 0; i < 4; i++)
      if (!PATTERNFLOW_METADATA[variant].controlLabel[i]) pfcParams.control[i] = PATTERNFLOW_METADATA[variant].defaults[i];
  }
  return true;
}

// The playing pattern's record, or 0 bytes for an engine without one.
size_t buildPatternRecord(uint8_t (&record)[PATTERN_RECORD_CAPACITY]) {
  const int engine = currentAnimation;
  void* controls;
  const size_t size = sizeof(PatternLook) + engineControls(engine, controls);
  if (!controls || size > sizeof(record)) return 0;
  PatternLook look = {colorParams[engine], {}};
  memcpy(look.stops, paletteStops[engine], sizeof(look.stops));
  memcpy(record, &look, sizeof(look));
  memcpy(record + sizeof(look), controls, size - sizeof(look));
  return size;
}

bool savePatternRecord() {
  uint8_t record[PATTERN_RECORD_CAPACITY];
  const size_t size = buildPatternRecord(record);
  Preferences store;
  const bool ok = size && store.begin("matrix-cfg", false) && PatternStore::write(store, currentPatternId(), record, size);
  store.end();
  return ok;
}

bool savePendingRecords() {
  Preferences store;
  const bool ok = store.begin("matrix-cfg", false) && pendingRecords.writeAll(store);
  store.end();
  return ok;
}

// Restarts the current pattern's animation from its live configuration.
void restartPattern() {
  patternflowPack.reset(currentAnimation == PATTERNFLOW_COLLECTION ? pfcParams.variant : 255);
  nativeEngines.reset();
  paletteTableAnimation = -1;
  patternStartedMs = millis();
  const unsigned long now = micros();
  lastFrameTime = now - FRAME_DURATION_US;
  nextFrameDueTime = now;
}

// Loads a pattern's stored configuration, or its compiled defaults without one.
void playPattern(int engine, int variant) {
  if (!isAvailableEngine(engine) || (engine == PATTERNFLOW_COLLECTION && !isAvailableVariant(variant))) return;
  currentAnimation = (Animation)engine;
  applyDefaults(engine, variant);
  readPatternRecord(engine, variant);
  restartPattern();
}

// Settings of another version are erased rather than converted: the matrix
// starts from defaults and keeps only its Wi-Fi networks and white balance.
void resetSettings(Preferences& store) {
  Serial.println("Settings from another firmware version; starting from defaults.");
  store.clear();
  store.putUInt("version", PatternStore::Version);
  // The Wi-Fi driver keeps its settings in RAM here, so its own store goes too.
  Preferences wifi;
  if (wifi.begin("nvs.net80211", false)) { wifi.clear(); wifi.end(); }
}

void loadSettings() {
  Preferences store;
  if (!store.begin("matrix-cfg", false)) return;
  const bool reset = store.getUInt("version", 0) != PatternStore::Version;
  if (reset) resetSettings(store);
  autoCycleEnabled = store.getBool("auto_cycle", autoCycleEnabled);
  autoCycleIntervalSecs = constrain(store.getULong("cycle_int", autoCycleIntervalSecs), 1UL, 86400UL);
  scheduleEnabled = store.getBool("sched_on", scheduleEnabled);
  scheduleStartMinutes = constrain(store.getUShort("sched_start", scheduleStartMinutes), 0, 1439);
  scheduleEndMinutes = constrain(store.getUShort("sched_end", scheduleEndMinutes), 0, 1439);
  scheduleFadeInSeconds = constrain(store.getUShort("sched_fadein", scheduleFadeInSeconds), 1, 3600);
  scheduleFadeOutSeconds = constrain(store.getUShort("sched_fadeout", scheduleFadeOutSeconds), 1, 3600);
  deviceBrightness = min(15, (int)store.getUChar("bright", deviceBrightness));
  store.end();
  if (reset && memcmp(panelBalance, DEFAULT_BALANCE, sizeof(panelBalance))) saveColorCalibration();
}

// Every pattern draws each pixel through here, so it runs from internal RAM and
// leaves the flash cache to the pattern's own code.
IRAM_ATTR void setMatrixPointColor(int y, int x, uint16_t color) {
  if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) return;
#if defined(MATRIX_PROFILE_WS2812)
  matrix.setLed(ledIndex[y][x], ledEncoder.encode(color, x, y, ledOutputGain));
#else
  uint16_t* canvas = matrix.getBuffer();
  if (canvas) canvas[y * SCREEN_WIDTH + x] = combinedOutputScale < 0.999f ? scale565(color, combinedOutputScale) : color;
#endif
}

IRAM_ATTR void drawPatternPixel(void*, int16_t x, int16_t y, uint16_t color) {
  setMatrixPointColor(y, x, color);
}

// Samples the palette loop at position, then applies Saturation in HSV.
uint16_t makePaletteColor(float position, int brightness, const ColorParams& controls) {
  position -= floorf(position);
  const float scaled = position * 4.0f;
  const int first = ((int)scaled) & 3;
  const int second = (first + 1) & 3;
  const float mix = scaled - floorf(scaled);
  const auto* stops = paletteStops[(int)currentAnimation];
  const uint32_t a = stops[first];
  const uint32_t b = stops[second];

  const float red = ((((a >> 16) & 0xFF) * (1.0f - mix)) +
                     (((b >> 16) & 0xFF) * mix)) / 255.0f;
  const float green = ((((a >> 8) & 0xFF) * (1.0f - mix)) +
                       (((b >> 8) & 0xFF) * mix)) / 255.0f;
  const float blue = (((a & 0xFF) * (1.0f - mix)) +
                      ((b & 0xFF) * mix)) / 255.0f;
  const float maximum = max(red, max(green, blue));
  const float delta = maximum - min(red, min(green, blue));
  const float sourceSaturation = maximum > 0.0001f ? delta / maximum : 0.0f;
  const float tunedSaturation = constrain(
    sourceSaturation * (controls.saturation / 100.0f),
    0.0f, 1.0f);
  return makeColor(rgbHue(red, green, blue, maximum, delta), tunedSaturation, maximum, brightness);
}

void ensurePaletteTable(int brightness) {
  brightness = constrain(brightness, 0, 15);
  const ColorParams& controls = colorParams[(int)currentAnimation];
  if (paletteTableAnimation == (int)currentAnimation &&
      paletteTableBrightness == brightness &&
      paletteTableParams.saturation == controls.saturation &&
      paletteTableParams.palette == controls.palette) return;

  for (int i = 0; i < 256; i++) {
    paletteTable[i] = makePaletteColor(i / 256.0f, brightness, controls);
  }
  paletteTableAnimation = (int)currentAnimation;
  paletteTableBrightness = brightness;
  paletteTableParams = controls;
}

// The share of full scale colours are drawn at: WS2812 renders full-scale and
// LedEncoder applies brightness in linear light, while HUB75 colours carry it.
float colorScale(int brightness) {
#if defined(MATRIX_PROFILE_WS2812)
  return brightness > 0 ? 1.0f : 0.0f;
#else
  return constrain(brightness, 0, 15) / 15.0f;
#endif
}

uint16_t makeColor(float hue, float saturation, float value, int brightness) {
  saturation = constrain(saturation, 0.0f, 1.0f);
  const float scale = colorScale(brightness) * constrain(value, 0.0f, 1.0f);

  hue = hue - floorf(hue); // Normalise safely to 0.0 <= hue < 1.0.

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

  return rgb565(r * scale, g * scale, b * scale);
}

// -----------------------------------------------------------------------------
// Wi-Fi storage and fallback access point
// -----------------------------------------------------------------------------
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

// Saves a network, or a new password for a known one, and returns its slot or -1.
int addOrUpdateNetwork(const String& ssid, const String& password) {
  if (ssid.length() == 0 || ssid.length() > 32 || password.length() > 64) return -1;

  if (!preferences.begin("wifi-list", false)) return -1;
  int count = preferences.getInt("count", 0);
  count = constrain(count, 0, (int)MAX_WIFI_NETWORKS);

  for (int i = 0; i < count; i++) {
    if (preferences.getString(("ssid_" + String(i)).c_str(), "") == ssid) {
      preferences.putString(("pass_" + String(i)).c_str(), password);
      const String key="pass_"+String(i);
      bool ok=preferences.isKey(key.c_str())&&preferences.getString(key.c_str())==password;
      preferences.end();
      return ok ? i : -1;
    }
  }

  if (count >= MAX_WIFI_NETWORKS) {
    preferences.end();
    return -1;
  }

  preferences.putString(("ssid_" + String(count)).c_str(), ssid);
  preferences.putString(("pass_" + String(count)).c_str(), password);
  bool ok=preferences.isKey(("ssid_"+String(count)).c_str())&&preferences.getString(("ssid_"+String(count)).c_str())==ssid&&
    preferences.isKey(("pass_"+String(count)).c_str())&&preferences.getString(("pass_"+String(count)).c_str())==password;
  if(ok)preferences.putInt("count", count + 1);
  ok=ok&&preferences.getInt("count",0)==count+1;
  preferences.end();
  return ok ? count : -1;
}

// The saved network the next boot tries first.
bool preferNetwork(int index) {
  Preferences store;
  if (!store.begin("wifi-list", false)) return false;
  store.putInt("preferred", index);
  const bool ok = store.getInt("preferred", -1) == index;
  store.end();
  return ok;
}

// Local time, the .local name and the OTA receiver, once a network is joined.
void startNetworkServices() {
  if (!mdnsStarted) {
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
  setupOta();
}

// The saved slot tried at a position in a round: the network joined last
// first, then the others in order, or -1 once the round is over.
int savedNetworkAt(int position, int preferred, int count) {
  if (preferred < 0 || preferred >= count) return position < count ? position : -1;
  if (position == 0) return preferred;
  const int slot = position - (position <= preferred);
  return slot < count ? slot : -1;
}

// Starts the next saved network of this round, or ends the round and returns false.
bool joinNextSavedNetwork() {
  preferences.begin("wifi-list", true);
  const int count = constrain(preferences.getInt("count", 0), 0, (int)MAX_WIFI_NETWORKS);
  const int preferred = preferences.getInt("preferred", -1);
  preferences.end();
  const int slot = savedNetworkAt(wifiRetryPosition, preferred, count);
  String ssid;
  String password;
  if (slot >= 0 && readWifiNetwork(slot, ssid, password)) {
    Serial.printf("Trying Wi-Fi: %s\n", ssid.c_str());
    wifiRetrySlot = slot;
    wifiRetryPosition++;
    WiFi.begin(ssid.c_str(), password.c_str());
    return true;
  }
  if (wifiRetryPosition) WiFi.disconnect(false, false);
  wifiRetryPosition = 0;
  return false;
}

// Joins a saved network in the background while patterns play, one connection
// timeout per network, and opens the setup network when none answers.
void startWifi() {
  isAPMode = false;
  WiFi.mode(WIFI_MODE_NULL);
  delay(100);
  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
#if !defined(MATRIX_PROFILE_WS2812)
  WiFi.setSleep(false);
#endif
  WiFi.setAutoReconnect(true);
  wifiRetryPosition = 0;
  wifiRetryAt = millis();
  wifiJoining = joinNextSavedNetwork();
  if (!wifiJoining) startAccessPoint();
}

// Records the network that answered, so the next boot tries it first.
void joinedSavedNetwork() {
  preferences.begin("wifi-list", true);
  const int preferred = preferences.getInt("preferred", -1);
  preferences.end();
  if (wifiRetrySlot >= 0 && wifiRetrySlot != preferred) preferNetwork(wifiRetrySlot);
  wifiRetryPosition = 0;
  Serial.print("Connected. IP: ");
  Serial.println(WiFi.localIP());
  startNetworkServices();
}

void startAccessPoint() {
  if (isAPMode) return;

  if (mdnsStarted) {
    MDNS.end();
    mdnsStarted = false;
  }

  // Initialise the radio as a station first. This is the same path used by a
  // successful scan and avoids the ESP32-S3 AP transition failure observed
  // after a station connection times out. softAP() then adds the AP interface.
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false, false);
  const bool stationModeStarted = WiFi.mode(WIFI_STA);
  delay(150);
  bool started = stationModeStarted &&
                 WiFi.softAP(SETUP_AP_SSID, nullptr, 1, false, 4);

  if (!started) {
    // Native IDF fallback: retain Arduino's initialised AP netif/DHCP server,
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
  wifiRetryPosition = 0;
  wifiRetryAt = millis();

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

// Stops a retry underway and restarts the pause before the next round.
void holdWifiRetries() {
  if (wifiRetryPosition) WiFi.disconnect(false, false);
  wifiRetryPosition = 0;
  wifiRetryAt = millis();
}

// While the setup network is open, the saved networks are retried in the
// background, each for one connection timeout, with a minute between rounds.
// A phone on the setup network stops an attempt at once and pauses the rounds
// until a minute after it leaves, since a station attempt moves the access
// point's channel and blocks scans. The setup network closes once one answers.
void retrySavedNetworks() {
  if (WiFi.softAPgetStationNum() > 0) {
    holdWifiRetries();
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.setAutoReconnect(true);
    isAPMode = false;
    joinedSavedNetwork();
    Serial.println("Setup network closed.");
    return;
  }
  const unsigned long now = millis();
  if (now - wifiRetryAt < (wifiRetryPosition ? WIFI_CONNECT_TIMEOUT_MS : WIFI_RETRY_INTERVAL_MS)) return;
  wifiRetryAt = now;
  joinNextSavedNetwork();
}

void maintainWifi() {
  static uint64_t previousNetwork = UINT64_MAX;
  static unsigned long checkedAt = 0;
  const unsigned long now = millis();
  if (previousNetwork == UINT64_MAX || now - checkedAt >= 250) {
    checkedAt = now;
    const uint32_t address = isAPMode ? uint32_t(WiFi.softAPIP()) : uint32_t(WiFi.localIP());
    const uint64_t network = (uint64_t(address) << 9) | (uint64_t(uint8_t(WiFi.status())) << 1) | isAPMode;
    if (network != previousNetwork) { previousNetwork = network; stateRevision++; }
  }
  if (isAPMode) {
    retrySavedNetworks();
    return;
  }

  if (wifiJoining) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiJoining = false;
      joinedSavedNetwork();
    } else if (now - wifiRetryAt >= WIFI_CONNECT_TIMEOUT_MS) {
      wifiRetryAt = now;
      wifiJoining = joinNextSavedNetwork();
      if (!wifiJoining) startAccessPoint();
    }
    return;
  }

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
// JSON helpers and web handlers
// -----------------------------------------------------------------------------
// Clamps a number from JSON; NaN and infinities fall back to the minimum.
float clampFloat(float value, float minimum, float maximum) {
  if (!isfinite(value)) return minimum;
  if (value < minimum) return minimum;
  if (value > maximum) return maximum;
  return value;
}

void addCurrentParamsToJson(JsonObject obj) {
  switch (currentAnimation) {
    case STARFIELD:
      obj["n"] = sfParams.n; obj["s"] = sfParams.s; obj["w"] = sfParams.w; break;
    case WARPED_CHECKERBOARD:
      obj["as"] = wcParams.as; obj["ws"] = wcParams.ws; obj["d"] = wcParams.d; break;
    case MOIRE_LINES:
      obj["as"] = mlParams.as; obj["rs1"] = mlParams.rs1; obj["rs2"] = mlParams.rs2; obj["lf"] = mlParams.lf; break;
    case SHOCKWAVE:
      obj["as"] = swParams.as; obj["wt"] = swParams.wt; break;
    case WARP_CORRIDORS:
      obj["as"] = wcoParams.as; obj["rs"] = wcoParams.rs; obj["ws"] = wcoParams.ws; obj["bf"] = wcoParams.bf; break;
    case PIXEL_AVALANCHE:
      obj["sr"] = paParams.sr; obj["as"] = paParams.as; break;
    case BAYER_FLOW:
      obj["as"] = bdParams.as; obj["scale"] = bdParams.scale; obj["contrast"] = bdParams.contrast; break;
    case SNAKE:
      obj["speed"] = snParams.speed; obj["length"] = snParams.length; obj["steering"] = snParams.steering; break;
    case SQUARES:
      obj["as"] = sqParams.as; obj["count"] = sqParams.count; obj["thickness"] = sqParams.thickness; obj["spacing"] = sqParams.spacing; obj["variance"] = sqParams.variance; break;
    case FLOW_SPHERE:
      obj["speed"] = fsParams.speed; obj["scale"] = fsParams.scale; obj["density"] = fsParams.density; obj["radius"] = fsParams.radius; break;
    case PATTERNFLOW_COLLECTION:
      obj["variant"] = pfcParams.variant;
      obj["c0"] = pfcParams.control[0]; obj["c1"] = pfcParams.control[1];
      obj["c2"] = pfcParams.control[2]; obj["c3"] = pfcParams.control[3]; break;
  }
  const ColorParams& color = colorParams[(int)currentAnimation];
  obj["saturation"] = color.saturation;
  obj["palette"] = color.palette;
  for (int colorIndex = 0; colorIndex < 4; colorIndex++) {
    char key[8];
    char hex[8];
    snprintf(key, sizeof(key), "color%d", colorIndex);
    snprintf(hex, sizeof(hex), "#%06lX",
             (unsigned long)(paletteStops[(int)currentAnimation][colorIndex] & 0xFFFFFF));
    obj[key] = hex;
  }
  JsonArray original = obj.createNestedArray("original");
  const int variant = currentAnimation == PATTERNFLOW_COLLECTION ? pfcParams.variant : 0;
  for (int colorIndex = 0; colorIndex < 4; colorIndex++) {
    char hex[8];
    snprintf(hex, sizeof(hex), "#%06lX",
             (unsigned long)(compiledStop((int)currentAnimation, variant, Palettes::Original, colorIndex) & 0xFFFFFF));
    original.add(hex);
  }
}

// Sets one control of the playing pattern.
bool setPatternParam(const char* name, JsonVariantConst value) {
  if (name == nullptr || !value.is<float>()) return false;
  const int engine = currentAnimation;
  const float f = value.as<float>();
  const int i = value.as<int>();
  bool changed = true;

  if (!strcmp(name, "saturation")) {
    colorParams[engine].saturation = constrain(i, 0, 200);
    paletteTableAnimation = -1;
    return true;
  }
  if (!strcmp(name, "palette")) {
    if (!Palettes::offered(i)) return false;
    if (colorParams[engine].palette != i) {
      colorParams[engine].palette = i;
      compileStops(paletteStops[engine], engine,
                   engine == PATTERNFLOW_COLLECTION ? pfcParams.variant : 0, i);
    }
    paletteTableAnimation = -1;
    return true;
  }

  switch (engine) {
    case STARFIELD:
      if(!strcmp(name,"n")) sfParams.n=constrain(i,10,MAX_STARS); else if(!strcmp(name,"s")) sfParams.s=clampFloat(f,2,10); else if(!strcmp(name,"w")) sfParams.w=clampFloat(f,.05f,1); else changed=false; break;
    case WARPED_CHECKERBOARD:
      if(!strcmp(name,"as")) wcParams.as=clampFloat(f,.1f,3); else if(!strcmp(name,"ws")) wcParams.ws=clampFloat(f,2,10); else if(!strcmp(name,"d")) wcParams.d=clampFloat(f,1,8); else changed=false; break;
    case MOIRE_LINES:
      if(!strcmp(name,"as")) mlParams.as=clampFloat(f,.1f,3); else if(!strcmp(name,"rs1")) mlParams.rs1=clampFloat(f,.1f,3); else if(!strcmp(name,"rs2")) mlParams.rs2=clampFloat(f,.1f,3); else if(!strcmp(name,"lf")) mlParams.lf=clampFloat(f,.1f,2); else changed=false; break;
    case SHOCKWAVE:
      if(!strcmp(name,"as")) swParams.as=clampFloat(f,.1f,3); else if(!strcmp(name,"wt")) swParams.wt=clampFloat(f,.1f,5); else changed=false; break;
    case WARP_CORRIDORS:
      if(!strcmp(name,"as")) wcoParams.as=clampFloat(f,.1f,3); else if(!strcmp(name,"rs")) wcoParams.rs=clampFloat(f,.1f,2); else if(!strcmp(name,"ws")) wcoParams.ws=clampFloat(f,.1f,2); else if(!strcmp(name,"bf")) wcoParams.bf=clampFloat(f,.1f,2); else changed=false; break;
    case PIXEL_AVALANCHE:
      if(!strcmp(name,"as")) paParams.as=clampFloat(f,.1f,3); else if(!strcmp(name,"sr")) paParams.sr=constrain(i,1,10); else changed=false; break;
    case BAYER_FLOW:
      if(!strcmp(name,"as")) bdParams.as=clampFloat(f,.1f,3); else if(!strcmp(name,"scale")) bdParams.scale=constrain(i,1,16); else if(!strcmp(name,"contrast")) bdParams.contrast=constrain(i,1,8); else changed=false; break;
    case SNAKE:
      if(!strcmp(name,"speed")) snParams.speed=clampFloat(f,1,20); else if(!strcmp(name,"length")) snParams.length=constrain(i,8,300); else if(!strcmp(name,"steering")) snParams.steering=clampFloat(f,0,1); else changed=false; break;
    case SQUARES:
      if(!strcmp(name,"as")) sqParams.as=clampFloat(f,.1f,4); else if(!strcmp(name,"count")) sqParams.count=constrain(i,2,20); else if(!strcmp(name,"thickness")) sqParams.thickness=constrain(i,1,4); else if(!strcmp(name,"spacing")) sqParams.spacing=clampFloat(f,.2f,2); else if(!strcmp(name,"variance")) sqParams.variance=clampFloat(f,0,.5f); else changed=false; break;
    case FLOW_SPHERE:
      if(!strcmp(name,"speed")) fsParams.speed=clampFloat(f,.05f,6); else if(!strcmp(name,"scale")) fsParams.scale=clampFloat(f,.5f,4); else if(!strcmp(name,"density")) fsParams.density=clampFloat(f,.1f,1); else if(!strcmp(name,"radius")) fsParams.radius=clampFloat(f,14,31); else changed=false; break;
    case PATTERNFLOW_COLLECTION:
      // A control without a label is fixed at its default.
      if(name[0]=='c' && name[1]>='0' && name[1]<='3' && name[2]=='\0' &&
         PATTERNFLOW_METADATA[pfcParams.variant].controlLabel[name[1]-'0'])
        pfcParams.control[name[1]-'0']=constrain(i,0,100);
      else changed=false;
      break;
  }

  return changed;
}

String makeCommandResultJson(bool success, const String& message) {
  DynamicJsonDocument result(256);
  result["success"] = success;
  result["message"] = message;
  String response;
  serializeJson(result, response);
  return response;
}

void sendJson(bool success, const String& message, int status = 200) {
  server.send(status, "application/json", makeCommandResultJson(success, message));
}

// Pages are stored gzipped; browsers revalidate them by ETag.
void sendCompressedPage(const uint8_t* page, size_t size, const char* etag) {
  server.sendHeader("ETag", etag);
  server.sendHeader("Cache-Control", "no-cache");
  if (server.header("If-None-Match") == etag) {
    server.send(304);
    return;
  }
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html; charset=utf-8", reinterpret_cast<const char*>(page), size);
}

void handleRoot() {
  // The app also holds the Wi-Fi controls, so it serves station,
  // setup-network and USB-recovery workflows alike.
  sendCompressedPage(APP_HTML_GZ, APP_HTML_GZ_SIZE, APP_ETAG);
}

void handlePreviewAtlas() {
  server.sendHeader("Cache-Control", "public, max-age=86400");
  server.send_P(200, "image/png",
                reinterpret_cast<const char*>(PATTERN_PREVIEW_ATLAS),
                PATTERN_PREVIEW_ATLAS_SIZE);
}

void handleSetupPage() {
  sendCompressedPage(SETUP_HTML_GZ, SETUP_HTML_GZ_SIZE, SETUP_ETAG);
}

String buildControlJson() {
  DynamicJsonDocument doc(6144);
  doc["previewFrames"]=PATTERN_PREVIEW_FRAMES;
  doc["previewWidth"]=PATTERN_PREVIEW_WIDTH;doc["previewHeight"]=PATTERN_PREVIEW_HEIGHT;
  doc["previewIntervalMs"]=PATTERN_PREVIEW_INTERVAL_MS;
  doc["previewAtlasKey"]=PATTERN_PREVIEW_ATLAS_KEY;
  doc["success"] = true;
  doc["currentPattern"] = (int)currentAnimation;
  doc["stateRevision"] = stateRevision;
  doc["brightness"] = deviceBrightness;
  doc["autoCycleEnabled"] = autoCycleEnabled;
  doc["cycleInterval"] = autoCycleIntervalSecs;
  doc["scheduleEnabled"] = scheduleEnabled;
  doc["scheduleStartMinutes"] = scheduleStartMinutes;
  doc["scheduleEndMinutes"] = scheduleEndMinutes;
  doc["scheduleFadeInSeconds"] = scheduleFadeInSeconds;
  doc["scheduleFadeOutSeconds"] = scheduleFadeOutSeconds;
  doc["deviceProfile"] = DEVICE_PROFILE_ID;
  doc["deviceName"] = DEVICE_DISPLAY_NAME;
  doc["projectName"] = "Matrix.hub";
  doc["projectAuthor"] = "Televator";
  doc["firmware"] = FIRMWARE_VERSION;
  doc["backend"] = DISPLAY_BACKEND_NAME;
  doc["displayWidth"] = SCREEN_WIDTH;
  doc["displayHeight"] = SCREEN_HEIGHT;
  JsonObject calibration=doc.createNestedObject("calibration");
  calibration["r"]=panelBalance[0];calibration["g"]=panelBalance[1];calibration["b"]=panelBalance[2];
  calibration["chart"]=testChartShown;
  JsonObject balance=calibration.createNestedObject("default");
  balance["r"]=DEFAULT_BALANCE[0];balance["g"]=DEFAULT_BALANCE[1];balance["b"]=DEFAULT_BALANCE[2];
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
  doc["pattern"] = playingName();
  doc["actualFPS"] = performance.actualFPS;
  doc["panelRefreshHz"] = performance.panelRefreshHz;
  doc["deviceProfile"] = DEVICE_PROFILE_ID;
  doc["backend"] = DISPLAY_BACKEND_NAME;
  doc["stateRevision"] = stateRevision;
  doc["averageRenderUs"] = performance.averageRenderUs;
  doc["averageDrawUs"] = performance.averageDrawUs;
  doc["averageShowUs"] = performance.averageShowUs;
  doc["worstRenderUs"] = performance.worstRenderUs;
  doc["overBudgetFrames"] = performance.overBudgetFrames;
  doc["sampleFrames"] = performance.sampleFrames;

  doc["renderBudgetPercent"] = performance.averageRenderUs * 100.0f / FRAME_DURATION_US;
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["minimumFreeHeap"] = ESP.getMinFreeHeap();
  doc["largestFreeBlock"] = ESP.getMaxAllocHeap();
  doc["cpuMHz"] = ESP.getCpuFreqMHz();
#if !defined(MATRIX_PROFILE_WS2812)
  doc["displayClockMHz"] = matrix.clockMHz();
  doc["refreshMeasured"] = matrix.counterReady();
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

// One configuration per pattern. Edits apply live and persist once they
// settle, before another pattern starts and when an update begins.
bool brightnessDirty = false;
unsigned long brightnessSaveDueMs = 0;

void markConfigEdited() {
  configDirty = true;
  configSaveDueMs = millis() + 3000;
  lastAnimationChangeTime = millis();  // Auto-cycle waits a full interval after an edit.
}

bool saveCurrentConfig() {
  const bool ok = savePatternRecord();
  configDirty = !ok;
  if (!ok) configSaveDueMs = millis() + 30000;  // Retry later without hammering flash.
  return ok;
}

// Saves pending edits before another pattern starts, or holds them for a retry.
void flushConfig() {
  if (!configDirty || saveCurrentConfig()) return;
  uint8_t record[PATTERN_RECORD_CAPACITY];
  pendingRecords.keep(currentPatternId(), record, buildPatternRecord(record));
  configDirty = false;
}

void setDeviceBrightness(int value) {
  deviceBrightness = (uint8_t)constrain(value, 0, 15);
  brightnessDirty = true;
  brightnessSaveDueMs = millis() + 2000;
}

void persistSettledSettings(bool immediately) {
  const unsigned long now = millis();
  if (immediately || (long)(now - configSaveDueMs) >= 0) {
    if (configDirty) saveCurrentConfig();
    if (!pendingRecords.empty() && !savePendingRecords()) configSaveDueMs = now + 30000;
  }
  if (brightnessDirty && (immediately || (long)(now - brightnessSaveDueMs) >= 0)) {
    Preferences store;
    brightnessDirty = !(store.begin("matrix-cfg", false) && store.putUChar("bright", deviceBrightness) == 1);
    store.end();
    if (brightnessDirty) brightnessSaveDueMs = now + 30000;
  }
}

// Restores the pattern's compiled defaults and forgets its stored record.
bool resetCurrentPattern() {
  configDirty = false;
  applyDefaults(currentAnimation, pfcParams.variant);
  restartPattern();
  Preferences store;
  const bool forgotten = store.begin("matrix-cfg", false) && PatternStore::forget(store, currentPatternId());
  store.end();
  if (!forgotten) markConfigEdited();  // Saving the defaults replaces the record instead.
  return forgotten;
}

// Plays a pattern now, after saving pending edits to the previous one.
void startPattern(int engine, int variant) {
  flushConfig();
  testChartShown = false;
  playPattern(engine, variant);
  transitionPhase = 0;
  transitionOpacity = 1.0f;
  lastAnimationChangeTime = millis();
}

// A hand-picked pattern stays: cycling stops, and the next boot resumes it.
bool rememberManualSelection(int engine, int variant) {
  Preferences store;
  if (!store.begin("matrix-cfg", false)) return false;
  bool ok = store.putUChar("last_pat", patternId(engine, variant)) == 1;
  if (autoCycleEnabled) {
    autoCycleEnabled = false;
    ok = store.putBool("auto_cycle", false) == 1 && ok;
  }
  store.end();
  return ok;
}

// Boot resumes the last pattern picked by hand unless it flashes, and starts
// the gallery otherwise.
void resumeLastPattern() {
  int id = CORE_PATTERN_IDS[0];
  Preferences store;
  if (store.begin("matrix-cfg", true)) {
    id = store.getUChar("last_pat", id);
    store.end();
  }
  const bool collection = id >= PATTERNFLOW_COLLECTION;
  const int engine = collection ? PATTERNFLOW_COLLECTION : id;
  const int variant = collection ? id - PATTERNFLOW_COLLECTION : 0;
  if (collection ? isAvailableVariant(variant) && !isFlashingVariant(variant) : isAvailableEngine(engine))
    startPattern(engine, variant);
  else
    startPattern(CORE_PATTERN_IDS[0], 0);
}

// Saves {"ssid":…,"password":…} as given and returns its slot or -1.
int saveNetworkJson(JsonVariantConst network) {
  if (!network["ssid"].is<const char*>() || !network["password"].is<const char*>()) return -1;
  return addOrUpdateNetwork(network["ssid"].as<String>(), network["password"].as<String>());
}

String processControlCommandJson(const String& body, int& status) {
  DynamicJsonDocument doc(1024);
  const DeserializationError error = deserializeJson(doc, body);
  if (error) {
    status = 400;
    return makeCommandResultJson(false, String("Invalid JSON: ") + error.c_str());
  }

  const char* commandName = doc["command"];
  if (!commandName) {
    status = 400;
    return makeCommandResultJson(false, "Missing command");
  }

  if (doc.containsKey("expectedPattern") &&
      (!doc["expectedPattern"].is<int>() || doc["expectedPattern"].as<int>() != (int)currentAnimation ||
       (currentAnimation == PATTERNFLOW_COLLECTION &&
        (!doc["expectedVariant"].is<int>() || doc["expectedVariant"].as<int>() != pfcParams.variant)))) {
    status = 409;
    return makeCommandResultJson(false, "The pattern changed; that edit was not applied.");
  }

  const JsonVariantConst value = doc["value"];
  // Settings are only stored whole.
  const bool incomplete =
    (!strcmp(commandName, "setAutoCycle") && !value.is<bool>()) ||
    (!strcmp(commandName, "setCycleInterval") && !value.is<long>()) ||
    (!strcmp(commandName, "setSchedule") &&
     !(doc["enabled"].is<bool>() && doc["startMinutes"].is<int>() && doc["endMinutes"].is<int>() &&
       doc["fadeInSeconds"].is<int>() && doc["fadeOutSeconds"].is<int>()));
  if (incomplete) {
    status = 400;
    return makeCommandResultJson(false, "Invalid value");
  }

  bool success = true;
  String message;

  if (!strcmp(commandName,"saveWifiNetwork")) {
    success=!wifiRestartAt&&saveNetworkJson(value)>=0;
    message=success?"Network saved.":"Could not save the network. Check its name; the matrix keeps up to eight.";
  } else if (!strcmp(commandName,"connectWifi")) {
    int index=value.as<int>();String ssid,password;
    success=value.is<int>()&&index>=0&&index<MAX_WIFI_NETWORKS&&!wifiRestartAt&&readWifiNetwork(index,ssid,password);
    success=success&&preferNetwork(index);
    if(success)wifiRestartAt=millis()+1500;
    message=success?"Restarting on the selected network.":"Could not select that network.";
  } else if (!strcmp(commandName, "setColorCalibration")) {
    const int r=value["r"]|-1,g=value["g"]|-1,b=value["b"]|-1;
    success=r>=0&&r<=100&&g>=0&&g<=100&&b>=0&&b<=100&&applyColorCalibration(r,g,b);
    if(success){panelBalance[0]=r;panelBalance[1]=g;panelBalance[2]=b;success=saveColorCalibration();}
    message=success?"White balance saved":"The matrix could not keep that white balance.";
  } else if (!strcmp(commandName,"setTestChart")) {
    success=value.is<bool>();
    if(success){testChartShown=value.as<bool>();testChartShownAt=millis();
      lastAnimationChangeTime=millis();transitionPhase=0;transitionOpacity=1.f;}
  } else if (!strcmp(commandName, "setPattern") || !strcmp(commandName, "setPatternVariant")) {
    const bool variant = !strcmp(commandName, "setPatternVariant");
    const int engine = variant ? PATTERNFLOW_COLLECTION : value.as<int>(), index = variant ? value.as<int>() : 0;
    const bool known = value.is<int>() && (variant ? isAvailableVariant(index)
                                                   : engine != PATTERNFLOW_COLLECTION && isAvailableEngine(engine));
    if (known) startPattern(engine, index);
    success = known && rememberManualSelection(engine, index);
    message = !known ? "Unknown pattern" : success ? String("Pattern: ") + playingName()
                                                   : "Playing, but the matrix could not remember the choice.";
  } else if (!strcmp(commandName, "setAutoCycle")) {
    autoCycleEnabled = value.as<bool>();
    success=preferences.begin("matrix-cfg",false) && preferences.putBool("auto_cycle",autoCycleEnabled)==1 && preferences.getBool("auto_cycle",!autoCycleEnabled)==autoCycleEnabled; preferences.end();
    lastAnimationChangeTime = millis();
    message = autoCycleEnabled ? "Cycling on" : "Cycling off";
  } else if (!strcmp(commandName, "setCycleInterval")) {
    autoCycleIntervalSecs = constrain(value.as<long>(), 1L, 86400L);
    success=preferences.begin("matrix-cfg",false) && preferences.putULong("cycle_int",autoCycleIntervalSecs)==sizeof(uint32_t) && preferences.getULong("cycle_int",0)==autoCycleIntervalSecs; preferences.end();
    lastAnimationChangeTime = millis();
    message = "Cycle interval saved";
  } else if (!strcmp(commandName, "setSchedule")) {
    scheduleEnabled = doc["enabled"].as<bool>();
    scheduleStartMinutes = constrain(doc["startMinutes"].as<int>(), 0, 1439);
    scheduleEndMinutes = constrain(doc["endMinutes"].as<int>(), 0, 1439);
    scheduleFadeInSeconds = constrain(doc["fadeInSeconds"].as<int>(), 1, 3600);
    scheduleFadeOutSeconds = constrain(doc["fadeOutSeconds"].as<int>(), 1, 3600);
    success=preferences.begin("matrix-cfg",false) &&
      preferences.putBool("sched_on",scheduleEnabled)==1 &&
      preferences.putUShort("sched_start",scheduleStartMinutes)==sizeof(uint16_t) &&
      preferences.putUShort("sched_end",scheduleEndMinutes)==sizeof(uint16_t) &&
      preferences.putUShort("sched_fadein",scheduleFadeInSeconds)==sizeof(uint16_t) &&
      preferences.putUShort("sched_fadeout",scheduleFadeOutSeconds)==sizeof(uint16_t) &&
      preferences.getBool("sched_on",!scheduleEnabled)==scheduleEnabled &&
      preferences.getUShort("sched_start",65535)==scheduleStartMinutes &&
      preferences.getUShort("sched_end",65535)==scheduleEndMinutes &&
      preferences.getUShort("sched_fadein",0)==scheduleFadeInSeconds &&
      preferences.getUShort("sched_fadeout",0)==scheduleFadeOutSeconds;
    preferences.end();
    message = "Schedule saved";
  } else if (!strcmp(commandName, "setParam")) {
    success = setPatternParam(doc["paramName"], value);
    if (success) markConfigEdited();
    message = success ? "Parameter updated" : "Unknown parameter";
  } else if (!strcmp(commandName, "setBrightness")) {
    success = value.is<int>();
    if (success) setDeviceBrightness(value.as<int>());
    message = success ? "Brightness set" : "Invalid value";
  } else if (!strcmp(commandName, "resetPattern")) {
    success = resetCurrentPattern();
    message = success ? "Pattern reset to its defaults" : "Reset applied, but saving failed";
  } else {
    success = false;
    message = "Unknown command";
  }

  if (!success && (!strcmp(commandName,"setAutoCycle") ||
      !strcmp(commandName,"setCycleInterval") || !strcmp(commandName,"setSchedule"))) message="The matrix could not save that setting.";
  if (message.isEmpty()) message = success ? "Updated" : "Invalid value";
  // A failed write may still change live values or earlier storage keys.
  stateRevision++;
  status = success ? 200 : 400;
  return makeCommandResultJson(success, message);
}

// Browsers send JSON across origins only after a preflight this server never
// answers, so requiring it keeps other web pages from posting commands.
bool jsonRequest() {
  if (server.header("Content-Type").startsWith("application/json")) return true;
  sendJson(false, "Commands must be sent as application/json.", 415);
  return false;
}

void handleControlPost() {
  if (!jsonRequest()) return;
  int status = 500;
  const String response = processControlCommandJson(server.arg("plain"), status);
  server.send(status, "application/json", response);
}

// The networks a finished scan found, once each, as the setup page lists them.
String wifiScanJson(int found) {
  DynamicJsonDocument doc(4096);
  doc["success"] = true;
  JsonArray networks = doc.createNestedArray("networks");
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
  WiFi.scanDelete();
  String response;
  serializeJson(doc, response);
  return response;
}

// Scans run in the background while frames play; the setup page asks again
// until the list is ready. A list answers until 30 s after its scan began, so
// every page waiting on that scan receives it while a list nobody collected in
// time is scanned again, and ?again always starts a new scan.
String wifiScanList;
unsigned long wifiScanStartedAt = 0;
bool wifiScanStarted = false;  // Until its list is collected.

void handleWifiScan() {
  static const char scanning[] = "{\"success\":true,\"scanning\":true}";
  static const char failed[] = "{\"success\":false,\"networks\":[]}";
  const int found = WiFi.scanComplete();
  if (found == WIFI_SCAN_RUNNING) {
    server.send(200, "application/json", scanning);
    return;
  }
  if (found >= 0) {
    wifiScanList = wifiScanJson(found);
    wifiScanStarted = false;
  } else if (wifiScanStarted) {
    wifiScanStarted = false;
    server.send(200, "application/json", failed);
    return;
  }
  if (wifiScanList.length() && millis() - wifiScanStartedAt < 30000 && !server.hasArg("again")) {
    server.send(200, "application/json", wifiScanList);
    return;
  }
  if (isAPMode) holdWifiRetries();  // A scan fails while a station attempt is underway.
  wifiScanList = String();
  wifiScanStartedAt = millis();
  wifiScanStarted = WiFi.scanNetworks(true, true) == WIFI_SCAN_RUNNING;
  server.send(200, "application/json", wifiScanStarted ? scanning : failed);
}

void handleWifiAdd() {
  if (!jsonRequest()) return;
  if(wifiRestartAt){sendJson(false,"The matrix is already restarting on another network.",409);return;}
  DynamicJsonDocument doc(512);
  if (deserializeJson(doc, server.arg("plain"))) {
    sendJson(false, "Invalid JSON", 400);
    return;
  }
  const bool restartAfterSave = doc["restart"] | isAPMode;
  // Joining from the setup page makes the new network the one the restart tries first.
  const int slot = saveNetworkJson(doc.as<JsonVariantConst>());
  const bool saved = slot >= 0 && (!restartAfterSave || preferNetwork(slot));
  stateRevision++;
  if (!saved) {
    sendJson(false, "Could not save the network. Check its name; the matrix keeps up to eight.", 400);
    return;
  }

  sendJson(true, restartAfterSave ? "Network saved. Restarting…" : "Network saved.");

  if (restartAfterSave) {
    delay(800);
    restartDevice();
  }
}

void handleWifiReset() {
  if (!jsonRequest()) return;
  if (!forgetWifiNetworks()) {
    sendJson(false, "Could not erase the saved networks. The matrix keeps running.", 500);
    return;
  }
  sendJson(true, "Networks forgotten. Restarting into the setup network…");
  delay(500);
  restartDevice();
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
  static const char* headers[] = {"If-None-Match", "Content-Type"};
  server.collectHeaders(headers, 2);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/preview-atlas.png", HTTP_GET, handlePreviewAtlas);
  server.on("/setup", HTTP_GET, handleSetupPage);
  server.on("/control", HTTP_GET, handleControlGet);
  server.on("/control", HTTP_POST, handleControlPost);
  server.on("/diagnostics", HTTP_GET, handleDiagnosticsGet);

  server.on("/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/wifi/add", HTTP_POST, handleWifiAdd);
  server.on("/wifi/reset", HTTP_POST, handleWifiReset);

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
