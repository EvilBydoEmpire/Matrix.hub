/*
  64x64 HUB75 RGB Matrix Controller
  -----------------------------------
  Merged version:
  - Ports the nine original patterns, LFO controls, three patch slots,
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

#include <Adafruit_Protomatter.h>
#include <Adafruit_GFX.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <math.h>
#include <esp_system.h>

#include "src/platform/ProtomatterBackend.h"
#include "src/patterns/HelloLuaPattern.h"
#include "src/patterns/LuaPatternRuntime.h"

// -----------------------------------------------------------------------------
// Hardware configuration — Adafruit MatrixPortal S3 + 64x64 HUB75
// -----------------------------------------------------------------------------
#define SCREEN_WIDTH    64
#define SCREEN_HEIGHT   64
#define DISPLAY_SCALE   2.0f

// Official MatrixPortal ESP32-S3 HUB75 pin mapping.
uint8_t rgbPins[]  = {42, 41, 40, 38, 39, 37}; // R1,G1,B1,R2,G2,B2
uint8_t addrPins[] = {45, 36, 48, 35, 21};     // A,B,C,D,E
uint8_t clockPin   = 2;
uint8_t latchPin   = 47;
uint8_t oePin      = 14;

// 5-bit color keeps refresh rate healthy on 64x64 while still allowing
// smooth, subdued color. Double buffering prevents visible tearing.
Adafruit_Protomatter matrix(
  SCREEN_WIDTH, 5, 1, rgbPins,
  5, addrPins,
  clockPin, latchPin, oePin,
  true
);

ProtomatterBackend displayBackend(matrix);
LuaPatternRuntime luaPattern(displayBackend);
bool luaHelloEnabled = false;

// -----------------------------------------------------------------------------
// Wi-Fi configuration
// -----------------------------------------------------------------------------
// The normal way to add Wi-Fi is through the setup access point. These optional
// compile-time credentials can be filled in as a backup. Leave them empty to
// avoid storing a Wi-Fi password in source code.
const char* DEFAULT_WIFI_SSID = "";
const char* DEFAULT_WIFI_PASSWORD = "";

const char* HOSTNAME = "64x64-rgb-matrix";
const char* SETUP_AP_SSID = "64x64-Matrix-Setup";
const char* SETUP_AP_PASSWORD = ""; // open setup access point

const byte DNS_PORT = 53;
const uint8_t MAX_WIFI_NETWORKS = 8;
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 7000;
const unsigned long WIFI_LOST_TO_AP_MS = 60000;

// -----------------------------------------------------------------------------
// Matrix, web server and global state
// -----------------------------------------------------------------------------
Preferences preferences;
WebServer server(80);
DNSServer dnsServer;

volatile bool isUpdatingWeb = false;
bool isAPMode = false;
bool mdnsStarted = false;
unsigned long wifiDisconnectedSince = 0;

int targetFPS = 45;
bool autoCycleEnabled = true;
unsigned long autoCycleIntervalSecs = 120;
unsigned long autoResyncInterval = 0; // HUB75/Protomatter: legacy MAX7219 re-sync disabled

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
  KALEIDOSCOPE
};

const int NUM_ANIMATIONS = 9;
const char* animationNames[NUM_ANIMATIONS] = {
  "Starfield", "Warped Checkerboard", "Moiré Rings", "Moiré Lines",
  "Warping Checkers", "Shockwave", "Warp Corridors", "Pixel Avalanche", "Kaleidoscope"
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

StarfieldParams sfParams;
WarpedCheckerboardParams wcParams;
MoireRingsParams mrParams;
MoireLinesParams mlParams;
WarpingCheckersParams wchParams;
ShockwaveParams swParams;
WarpCorridorsParams wcoParams;
PixelAvalancheParams paParams;
KaleidoscopeParams kdParams;

StarfieldParams sfPatches[NUM_PATCH_SLOTS];
WarpedCheckerboardParams wcPatches[NUM_PATCH_SLOTS];
MoireRingsParams mrPatches[NUM_PATCH_SLOTS];
MoireLinesParams mlPatches[NUM_PATCH_SLOTS];
WarpingCheckersParams wchPatches[NUM_PATCH_SLOTS];
ShockwaveParams swPatches[NUM_PATCH_SLOTS];
WarpCorridorsParams wcoPatches[NUM_PATCH_SLOTS];
PixelAvalancheParams paPatches[NUM_PATCH_SLOTS];
KaleidoscopeParams kdPatches[NUM_PATCH_SLOTS];

int defaultPatches[NUM_ANIMATIONS] = {0};

struct Star { float x, y, z; };
Star stars[MAX_STARS];
float distMap[SCREEN_HEIGHT][SCREEN_WIDTH];
byte avalancheGrid[SCREEN_HEIGHT][SCREEN_WIDTH] = {0};
float avalancheTimeSinceUpdate = 0;

unsigned long lastFrameTime = 0;

// -----------------------------------------------------------------------------
// Forward declarations
// -----------------------------------------------------------------------------
void setupWebServer();
void applyCurrentBrightness();
void loadPatch(Animation anim, int patchIndex);
void setMatrixPoint(int y, int x, bool state);
void setMatrixPointColor(int y, int x, uint16_t color);
uint16_t makeColor(float hue, float saturation, float value, int brightness);
uint16_t makeWhite(float value, int brightness);
void resyncDisplay();
void loadSettings();
void startAccessPoint();
bool connectToKnownWifi();
void maintainWifi();
void handleRoot();
void handleControlGet();
void handleControlPost();
void handleWifiAdd();
void handleWifiReset();
void handleWifiScan();
void handleSetupSave();
void handleNotFound();

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\n64x64 RGB Matrix Controller — MatrixPortal S3 Wi-Fi/UI version");

  ProtomatterStatus matrixStatus = matrix.begin();
  Serial.print("Protomatter begin() status: ");
  Serial.println((int)matrixStatus);
  if (matrixStatus != PROTOMATTER_OK) {
    Serial.println("ERROR: HUB75 matrix initialization failed.");
    while (true) delay(100);
  }
  matrix.fillScreen(0);
  matrix.show();

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
  applyCurrentBrightness();

  randomSeed((uint32_t)esp_random());

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

  if (!connectToKnownWifi()) {
    startAccessPoint();
  }

  server.begin();
  Serial.println("HTTP server started");

  lastAnimationChangeTime = millis();
  lastResyncTime = millis();
  lastFrameTime = micros();
}

// -----------------------------------------------------------------------------
// Main loop — animation functionality retained from the first sketch
// -----------------------------------------------------------------------------
void loop() {
  server.handleClient();
  if (isAPMode) dnsServer.processNextRequest();
  maintainWifi();

  if (autoResyncInterval > 0 &&
      (millis() - lastResyncTime >= autoResyncInterval * 1000UL)) {
    resyncDisplay();
    lastResyncTime = millis();
  }

  if (autoCycleEnabled &&
      (millis() - lastAnimationChangeTime >= autoCycleIntervalSecs * 1000UL)) {
    luaHelloEnabled = false;
    currentAnimation = (Animation)((currentAnimation + 1) % NUM_ANIMATIONS);
    loadPatch(currentAnimation, defaultPatches[currentAnimation]);
    lastAnimationChangeTime = millis();
  }

  if (isUpdatingWeb) return;

  const unsigned long currentTime = micros();
  const unsigned long frameDuration =
    (targetFPS > 0) ? (1000000UL / (unsigned long)targetFPS) : 1000000UL;

  if ((unsigned long)(currentTime - lastFrameTime) < frameDuration) return;

  float deltaTime = (currentTime - lastFrameTime) / 1000000.0f;
  if (deltaTime > 0.12f) deltaTime = 0.12f; // avoid giant simulation jumps
  lastFrameTime = currentTime;
  animationTime += deltaTime;

  if (luaHelloEnabled) {
    if (luaPattern.frame(animationTime, deltaTime)) {
      displayBackend.show();
      return;
    }

    luaHelloEnabled = false;
    Serial.print("Hello Lua stopped: ");
    Serial.println(luaPattern.lastError());
  }

  matrix.fillScreen(0);

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
      for (int i = 0; i < starCount; i++) {
        stars[i].z -= modSf.w * 50.0f * deltaTime * modSf.as;
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
          makeColor(hue, 0.42f, 0.35f + 0.65f * nearFactor, modSf.b));
      }
      break;
    }

    case WARPED_CHECKERBOARD: {
      const float time = animationTime * modWc.as;
      const float waveScale = max(1.0f, modWc.ws * DISPLAY_SCALE);
      const float distortion = modWc.d * DISPLAY_SCALE;

      for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
          const float warp = sinf(distMap[y][x] / waveScale - time * modWc.wsp) * distortion;
          const bool on = (((x + y + (int)floorf(warp)) & 1) == 0);
          if (on) {
            const float hue = 0.69f + 0.055f * sinf(time * 0.22f + distMap[y][x] * 0.075f);
            setMatrixPointColor(y, x, makeColor(hue, 0.50f, 0.78f, modWc.b));
          }
        }
      }
      break;
    }

    case MOIRE_RINGS: {
      const float time = animationTime * modMr.as;
      const float orbit = min(30.0f, modMr.orb * DISPLAY_SCALE);
      const float ringStep = max(2.0f, modMr.rt * DISPLAY_SCALE);
      const float phaseShift = time * modMr.es * DISPLAY_SCALE;

      const float cx1 = cx + orbit * sinf(time * 0.10f);
      const float cy1 = cy + orbit * cosf(time * 0.14f);
      const float cx2 = cx - orbit * sinf(time * 0.10f);
      const float cy2 = cy - orbit * cosf(time * 0.14f);

      for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
          const float d1 = hypotf(x - cx1, y - cy1);
          const float d2 = hypotf(x - cx2, y - cy2);
          const float p1 = fmodf(d1 + phaseShift, ringStep);
          const float p2 = fmodf(d2 + phaseShift, ringStep);
          const bool ring1 = p1 < ringStep * 0.50f;
          const bool ring2 = p2 < ringStep * 0.50f;

          if (ring1 != ring2) {
            const float hue = 0.48f + (d1 - d2) * 0.008f + time * 0.016f;
            setMatrixPointColor(y, x, makeColor(hue, 0.52f, 0.78f, modMr.b));
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

      for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
          const float px = x - cx;
          const float py = y - cy;
          const float v1 = px * c1 + py * s1;
          const float v2 = px * c2 + py * s2;
          const bool wave1 = sinf(v1 * spatial + time * 0.18f) > 0.0f;
          const bool wave2 = sinf(v2 * spatial - time * 0.15f) > 0.0f;

          if (wave1 != wave2) {
            const float hue = 0.57f + (v1 - v2) * 0.0035f + time * 0.010f;
            setMatrixPointColor(y, x, makeColor(hue, 0.42f, 0.76f, modMl.b));
          }
        }
      }
      break;
    }

    case WARPING_CHECKERS: {
      const float time = animationTime * modWch.as;
      const float centerX = cx + 26.0f * sinf(time * modWch.os * 0.17f);
      const float centerY = cy + 26.0f * cosf(time * modWch.os * 0.13f);
      const float ringStep = max(2.0f, modWch.rt * DISPLAY_SCALE);

      for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
          const float d = hypotf(x - centerX, y - centerY);
          const int band = (int)floorf(d / ringStep + time * modWch.es);
          if ((band & 1) == 0) {
            const float hue = 0.78f + band * 0.022f + time * 0.010f;
            setMatrixPointColor(y, x, makeColor(hue, 0.44f, 0.76f, modWch.b));
          }
        }
      }
      break;
    }

    case SHOCKWAVE: {
      const float time = animationTime * modSw.as;
      const float wavelength = max(0.5f, modSw.wt * DISPLAY_SCALE);
      for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
          const float wave = sinf(distMap[y][x] / wavelength - time * modSw.ws);
          const int phase = (wave > 0.0f) ? 1 : 0;
          if (((x + y) & 1) == phase) {
            const float hue = 0.02f + 0.08f * (wave + 1.0f) + time * 0.006f;
            setMatrixPointColor(y, x,
              makeColor(hue, 0.38f, 0.58f + fabsf(wave) * 0.24f, modSw.b));
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

      for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
          const float px = x - cx;
          const float py = y - cy;
          const float rotX = px * cosA - py * sinA;
          const float rotY = px * sinA + py * cosA;
          const float perspective = 1.0f + fabsf(rotY) * 0.012f;
          const float wave = sinf(rotX * spatial * perspective + phase);

          if (wave > 0.0f) {
            const float hue = 0.51f + rotY * 0.0025f + time * 0.010f;
            setMatrixPointColor(y, x,
              makeColor(hue, 0.48f, 0.62f + 0.22f * wave, modWco.b));
          }
        }
      }
      break;
    }

    case PIXEL_AVALANCHE: {
      avalancheTimeSinceUpdate += deltaTime;
      const float updateInterval = 1.0f / max(1.0f, 18.0f * modPa.as);

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

        // Reset only when the accumulated pile actually blocks the top row.
        int occupiedTop = 0;
        for (int x = 0; x < SCREEN_WIDTH; x++) occupiedTop += avalancheGrid[0][x] ? 1 : 0;
        if (occupiedTop > SCREEN_WIDTH / 3) {
          memset(avalancheGrid, 0, sizeof(avalancheGrid));
        }
      }

      for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
          if (avalancheGrid[y][x]) {
            const float vertical = (float)y / (SCREEN_HEIGHT - 1);
            const float hue = 0.08f + 0.12f * vertical;
            setMatrixPointColor(y, x,
              makeColor(hue, 0.50f, 0.84f - 0.16f * vertical, modPa.b));
          }
        }
      }
      break;
    }

    case KALEIDOSCOPE: {
      const float time = animationTime * modKd.as;

      auto plotK = [&](int py, int px, uint16_t color) {
        setMatrixPointColor(py, px, color);
        // A tiny 2-pixel sparkle makes the pattern readable on a 64x64 panel
        // without turning it into a large solid shape.
        setMatrixPointColor(py, px + 1, color);
      };

      auto drawK = [&](float x, float y, uint16_t color) {
        const int ix = (int)roundf(x);
        const int iy = (int)roundf(y);
        plotK((int)roundf(cy + iy), (int)roundf(cx + ix), color);
        plotK((int)roundf(cy - iy), (int)roundf(cx + ix), color);
        plotK((int)roundf(cy + iy), (int)roundf(cx - ix), color);
        plotK((int)roundf(cy - iy), (int)roundf(cx - ix), color);
        plotK((int)roundf(cy + ix), (int)roundf(cx + iy), color);
        plotK((int)roundf(cy - ix), (int)roundf(cx + iy), color);
        plotK((int)roundf(cy + ix), (int)roundf(cx - iy), color);
        plotK((int)roundf(cy - ix), (int)roundf(cx - iy), color);
      };

      const int points = constrain(modKd.p, 1, 15);
      for (int i = 0; i < points; i++) {
        const float angle = time * modKd.rs * 0.35f + i * (PI * 2.0f / points);
        const float radius = 20.0f + 9.0f * sinf(time * modKd.es * 0.45f + i * 0.9f);
        const float hue = 0.70f + i * 0.055f + time * 0.012f;
        drawK(radius * cosf(angle), radius * sinf(angle),
              makeColor(hue, 0.46f, 0.86f, modKd.b));
      }
      break;
    }
  }

  matrix.show();
}

// -----------------------------------------------------------------------------
// Persistence and matrix utilities
// -----------------------------------------------------------------------------
void initializeDefaultPatches() {
  sfParams  = {5, 120, 1.0f, 4.0f, 0.2f, 0, 0.5f, 1.0f};
  wcParams  = {6, 1.0f, 5.0f, 4.0f, 3.0f, 0, 0.5f, 1.0f};
  mrParams  = {6, 1.0f, 14.0f, 8.0f, 0.2f, 0, 0.5f, 1.0f};
  mlParams  = {6, 1.0f, 1.5f, 1.2f, 0.6f, 0, 0.5f, 1.0f};
  wchParams = {6, 1.0f, 1.0f, 3.0f, 0.5f, 0, 0.5f, 1.0f};
  swParams  = {5, 1.0f, 2.5f, 2.0f, 0, 0.5f, 1.0f};
  wcoParams = {6, 1.0f, 0.25f, 0.6f, 1.0f, 0, 0.5f, 1.0f};
  paParams  = {7, 4, 1.0f, 0, 0.5f, 1.0f};
  kdParams  = {7, 7, 1.0f, 1.0f, 1.0f, 0, 0.5f, 1.0f};

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
  }
  memset(defaultPatches, 0, sizeof(defaultPatches));
}

void saveAllMatrixSettings() {
  preferences.begin("matrix-cfg", false);
  preferences.putUInt("version", 7); // fixed 64x64 pattern engine / clean patch migration
  preferences.putInt("global_fps", targetFPS);
  preferences.putBool("auto_cycle", autoCycleEnabled);
  preferences.putULong("cycle_int", autoCycleIntervalSecs);
  preferences.putULong("resync_int", autoResyncInterval);
  preferences.putBytes("sf_p", sfPatches, sizeof(sfPatches));
  preferences.putBytes("wc_p", wcPatches, sizeof(wcPatches));
  preferences.putBytes("mr_p", mrPatches, sizeof(mrPatches));
  preferences.putBytes("ml_p", mlPatches, sizeof(mlPatches));
  preferences.putBytes("wch_p", wchPatches, sizeof(wchPatches));
  preferences.putBytes("sw_p", swPatches, sizeof(swPatches));
  preferences.putBytes("wco_p", wcoPatches, sizeof(wcoPatches));
  preferences.putBytes("pa_p", paPatches, sizeof(paPatches));
  preferences.putBytes("kd_p", kdPatches, sizeof(kdPatches));
  preferences.putBytes("def_p", defaultPatches, sizeof(defaultPatches));
  preferences.end();
}

void loadSettings() {
  preferences.begin("matrix-cfg", true);
  const bool valid = preferences.getUInt("version", 0) == 7;

  if (valid) {
    targetFPS = preferences.getInt("global_fps", 45);
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
      preferences.getBytes("def_p", defaultPatches, sizeof(defaultPatches)) == sizeof(defaultPatches);
    preferences.end();

    if (!complete) {
      Serial.println("Stored patch data was incomplete; restoring defaults.");
      initializeDefaultPatches();
      saveAllMatrixSettings();
    } else {
      Serial.println("Loaded matrix settings and patches from flash.");
    }
  } else {
    preferences.end();
    Serial.println("No compatible settings found; creating defaults.");
    initializeDefaultPatches();
    saveAllMatrixSettings();
  }

  targetFPS = constrain(targetFPS, 1, 60);
  autoCycleIntervalSecs = constrain(autoCycleIntervalSecs, 1UL, 86400UL);
  autoResyncInterval = constrain(autoResyncInterval, 0UL, 86400UL);

  // Migration from the MAX7219 version: periodic re-sync is unnecessary on
  // Protomatter and the old implementation could blank the panel.
  if (autoResyncInterval != 0) {
    autoResyncInterval = 0;
    preferences.begin("matrix-cfg", false);
    preferences.putULong("resync_int", 0);
    preferences.end();
    Serial.println("Disabled legacy automatic display re-sync for HUB75.");
  }

  for (int i = 0; i < NUM_ANIMATIONS; i++) {
    defaultPatches[i] = constrain(defaultPatches[i], 0, NUM_PATCH_SLOTS - 1);
  }

  loadPatch(STARFIELD, defaultPatches[STARFIELD]);
}

void applyCurrentBrightness() {
  // HUB75/Protomatter has no MAX7219-style global intensity register.
  // Brightness is applied in makeColor()/makeWhite() per rendered pixel.
}

void loadPatch(Animation anim, int patchIndex) {
  if (patchIndex < 0 || patchIndex >= NUM_PATCH_SLOTS) return;

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
  }

  if (anim == PIXEL_AVALANCHE) {
    memset(avalancheGrid, 0, sizeof(avalancheGrid));
    avalancheTimeSinceUpdate = 0;
  }

  applyCurrentBrightness();
  const unsigned long frameDuration =
    (targetFPS > 0) ? (1000000UL / (unsigned long)targetFPS) : 1000000UL;
  lastFrameTime = micros() - frameDuration;
}

void setMatrixPoint(int y, int x, bool state) {
  if (!state) return;
  setMatrixPointColor(y, x, makeWhite(0.75f, 5));
}

void setMatrixPointColor(int y, int x, uint16_t color) {
  if (y < 0 || y >= SCREEN_HEIGHT || x < 0 || x >= SCREEN_WIDTH) return;
  matrix.drawPixel(x, y, color);
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
  return matrix.color565(rr, gg, bb);
}

void resyncDisplay() {
  // The old MAX7219 project needed a hardware re-sync. Protomatter does not.
  // NEVER clear/show a black framebuffer here: that can leave the HUB75 panel
  // black if a web request blocks immediately afterward.
  Serial.println("Refreshing HUB75 frame timing (no framebuffer clear)...");
  const unsigned long frameDuration =
    (targetFPS > 0) ? (1000000UL / (unsigned long)targetFPS) : 1000000UL;
  lastFrameTime = micros() - frameDuration; // force a fresh frame on next loop
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

bool addOrUpdateNetwork(const String& ssid, const String& password) {
  if (ssid.length() == 0 || ssid.length() > 32 || password.length() > 64) return false;

  preferences.begin("wifi-list", false);
  int count = preferences.getInt("count", 0);
  count = constrain(count, 0, (int)MAX_WIFI_NETWORKS);

  for (int i = 0; i < count; i++) {
    if (preferences.getString(("ssid_" + String(i)).c_str(), "") == ssid) {
      preferences.putString(("pass_" + String(i)).c_str(), password);
      preferences.end();
      return true;
    }
  }

  if (count >= MAX_WIFI_NETWORKS) {
    preferences.end();
    return false;
  }

  preferences.putString(("ssid_" + String(count)).c_str(), ssid);
  preferences.putString(("pass_" + String(count)).c_str(), password);
  preferences.putInt("count", count + 1);
  preferences.end();
  return true;
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
  WiFi.setAutoReconnect(true);

  preferences.begin("wifi-list", true);
  int count = preferences.getInt("count", 0);
  preferences.end();
  count = constrain(count, 0, (int)MAX_WIFI_NETWORKS);

  for (int i = 0; i < count; i++) {
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

  WiFi.disconnect(false, false);
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_MODE_NULL);
  delay(100);
  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_AP_STA); // AP plus station mode allows Wi-Fi scanning

  const bool started = WiFi.softAP(SETUP_AP_SSID);
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
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#090b12">
<title>64×64 RGB Matrix Studio</title>
<style>
:root{color-scheme:dark;--bg:#080a10;--panel:rgba(20,24,36,.82);--panel2:#151a27;--line:rgba(255,255,255,.09);--text:#f3f6ff;--muted:#9099ad;--a:#7c5cff;--b:#18d5c2;--good:#27d17f;--warn:#ffb454;--bad:#ff5d73;--shadow:0 24px 80px rgba(0,0,0,.45)}
*{box-sizing:border-box}body{margin:0;min-height:100vh;font-family:Inter,ui-sans-serif,system-ui,-apple-system,"Segoe UI",sans-serif;background:radial-gradient(circle at 15% 0%,rgba(124,92,255,.19),transparent 34%),radial-gradient(circle at 90% 12%,rgba(24,213,194,.13),transparent 30%),var(--bg);color:var(--text)}
body:before{content:"";position:fixed;inset:0;pointer-events:none;background-image:linear-gradient(rgba(255,255,255,.018) 1px,transparent 1px),linear-gradient(90deg,rgba(255,255,255,.018) 1px,transparent 1px);background-size:32px 32px;mask-image:linear-gradient(to bottom,black,transparent 75%)}
.shell{width:min(1180px,calc(100% - 28px));margin:0 auto;padding:24px 0 48px}.top{display:flex;align-items:center;justify-content:space-between;gap:18px;margin-bottom:20px}.brand{display:flex;gap:14px;align-items:center}.logo{width:50px;height:50px;border-radius:16px;background:linear-gradient(135deg,var(--a),var(--b));display:grid;place-items:center;box-shadow:0 12px 35px rgba(124,92,255,.28)}.matrixIcon{display:grid;grid-template-columns:repeat(4,4px);gap:3px}.matrixIcon i{width:4px;height:4px;border-radius:50%;background:white;opacity:.92}.title h1{font-size:clamp(1.25rem,3vw,1.8rem);margin:0;letter-spacing:-.035em}.title p{margin:4px 0 0;color:var(--muted);font-size:.9rem}.status{display:flex;gap:8px;flex-wrap:wrap;justify-content:flex-end}.badge{display:inline-flex;align-items:center;gap:7px;padding:8px 11px;border:1px solid var(--line);border-radius:999px;background:rgba(255,255,255,.045);color:var(--muted);font-size:.78rem}.dot{width:8px;height:8px;border-radius:50%;background:var(--good);box-shadow:0 0 0 4px rgba(39,209,127,.12)}
.grid{display:grid;grid-template-columns:minmax(0,1.42fr) minmax(300px,.78fr);gap:18px}.card{background:var(--panel);border:1px solid var(--line);border-radius:22px;box-shadow:var(--shadow);backdrop-filter:blur(18px);overflow:hidden}.cardHead{display:flex;align-items:flex-start;justify-content:space-between;gap:12px;padding:20px 20px 15px;border-bottom:1px solid var(--line)}.cardHead h2{font-size:1rem;margin:0}.cardHead p{font-size:.78rem;color:var(--muted);margin:5px 0 0}.cardBody{padding:20px}.stack{display:grid;gap:18px}.selectWrap{display:grid;grid-template-columns:1fr auto;gap:10px}select,input[type=text],input[type=password],input[type=number]{width:100%;border:1px solid var(--line);background:#101522;color:var(--text);border-radius:12px;padding:12px 13px;outline:none}select:focus,input:focus{border-color:rgba(124,92,255,.75);box-shadow:0 0 0 3px rgba(124,92,255,.12)}
.controls{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px}.control{padding:14px;border:1px solid var(--line);background:rgba(255,255,255,.025);border-radius:16px}.controlTop{display:flex;justify-content:space-between;gap:10px;align-items:center;margin-bottom:10px}.control label{font-size:.82rem;font-weight:650}.value{font-variant-numeric:tabular-nums;color:var(--b);font-size:.78rem;background:rgba(24,213,194,.09);border:1px solid rgba(24,213,194,.17);padding:4px 7px;border-radius:8px}.help{margin:8px 0 0;color:var(--muted);font-size:.7rem;line-height:1.35}.rangeRow{display:grid;grid-template-columns:1fr 72px;gap:10px;align-items:center}input[type=range]{width:100%;accent-color:var(--a)}.miniInput{padding:8px!important;text-align:right;font-variant-numeric:tabular-nums}.sectionTitle{font-size:.75rem;text-transform:uppercase;letter-spacing:.14em;color:var(--muted);margin:6px 0 10px}.lfoBox{margin-top:16px;padding:16px;border-radius:18px;border:1px solid rgba(124,92,255,.25);background:linear-gradient(135deg,rgba(124,92,255,.08),rgba(24,213,194,.035))}.lfoGrid{display:grid;grid-template-columns:1.1fr 1fr 1fr;gap:12px}.lfoGrid .control{padding:12px}
.patchGrid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}.patch{padding:12px;border:1px solid var(--line);border-radius:15px;background:rgba(255,255,255,.025)}.patch strong{display:flex;align-items:center;justify-content:space-between;font-size:.82rem}.defaultMark{font-size:.62rem;color:var(--b);opacity:0}.patch.isDefault .defaultMark{opacity:1}.buttonRow{display:grid;grid-template-columns:1fr 1fr;gap:7px;margin-top:10px}.btn{appearance:none;border:0;border-radius:11px;padding:10px 12px;color:white;font-weight:700;font-size:.76rem;cursor:pointer;background:#252d40;transition:transform .15s,filter .15s}.btn:hover{filter:brightness(1.14)}.btn:active{transform:translateY(1px)}.primary{background:linear-gradient(135deg,var(--a),#5c79ff)}.teal{background:linear-gradient(135deg,#0ba896,var(--b));color:#06110f}.danger{background:rgba(255,93,115,.12);border:1px solid rgba(255,93,115,.28);color:#ff8fa0}.ghost{background:rgba(255,255,255,.055);border:1px solid var(--line)}.wide{width:100%}
.setting{display:grid;gap:8px}.settingHeader{display:flex;justify-content:space-between;align-items:center}.settingHeader label{font-size:.82rem;font-weight:650}.settingHeader span{font-size:.72rem;color:var(--muted)}.toggle{position:relative;width:46px;height:26px;display:inline-block}.toggle input{display:none}.toggle b{position:absolute;inset:0;border-radius:99px;background:#2b3140;border:1px solid var(--line);transition:.2s}.toggle b:after{content:"";position:absolute;width:20px;height:20px;left:2px;top:2px;border-radius:50%;background:white;transition:.2s}.toggle input:checked+b{background:linear-gradient(135deg,var(--a),var(--b))}.toggle input:checked+b:after{transform:translateX(20px)}.settingsGrid{display:grid;gap:15px}.two{display:grid;grid-template-columns:1fr 1fr;gap:10px}.networkInfo{padding:13px;border:1px solid var(--line);border-radius:14px;background:rgba(255,255,255,.025);font-size:.8rem;color:var(--muted);line-height:1.7}.networkInfo strong{color:var(--text)}.divider{height:1px;background:var(--line);margin:17px 0}.toast{position:fixed;right:18px;bottom:18px;max-width:min(380px,calc(100% - 36px));padding:13px 16px;border-radius:14px;background:#181e2b;border:1px solid var(--line);box-shadow:var(--shadow);transform:translateY(25px);opacity:0;pointer-events:none;transition:.25s;font-size:.82rem}.toast.show{transform:none;opacity:1}.toast.ok{border-color:rgba(39,209,127,.35)}.toast.bad{border-color:rgba(255,93,115,.4)}
@media(max-width:840px){.grid{grid-template-columns:1fr}.top{align-items:flex-start}.status{max-width:48%}}@media(max-width:600px){.shell{width:min(100% - 18px,1180px);padding-top:12px}.top{flex-direction:column}.status{max-width:none;justify-content:flex-start}.controls{grid-template-columns:1fr}.lfoGrid{grid-template-columns:1fr}.patchGrid{grid-template-columns:1fr}.cardHead,.cardBody{padding:16px}.selectWrap{grid-template-columns:1fr}.two{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="shell">
  <header class="top">
    <div class="brand"><div class="logo"><span class="matrixIcon"><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i></span></div><div class="title"><h1>64×64 RGB Matrix Studio</h1><p>MatrixPortal S3 · HUB75 pattern controller</p></div></div>
    <div class="status"><span class="badge"><i class="dot"></i><span id="modeBadge">Connecting</span></span><span class="badge" id="ipBadge">—</span></div>
  </header>
  <main class="grid">
    <div class="stack">
      <section class="card">
        <div class="cardHead"><div><h2>Pattern engine</h2><p>Live controls for the original nine animations</p></div></div>
        <div class="cardBody">
          <div class="selectWrap"><select id="pattern"></select><button class="btn ghost" onclick="refreshState()">Refresh</button></div>
          <div class="sectionTitle" style="margin-top:20px">Pattern parameters</div>
          <div id="paramControls" class="controls"></div>
          <div class="lfoBox"><div class="sectionTitle" style="margin-top:0">Low-frequency modulation</div><div id="lfoControls" class="lfoGrid"></div></div>
        </div>
      </section>
      <section class="card">
        <div class="cardHead"><div><h2>Patch memory</h2><p>Load, save, or make a patch the auto-cycle default</p></div></div>
        <div class="cardBody"><div id="patches" class="patchGrid"></div></div>
      </section>
    </div>
    <aside class="stack">
      <section class="card">
        <div class="cardHead"><div><h2>Playback</h2><p>Frame timing and maintenance</p></div></div>
        <div class="cardBody settingsGrid">
          <div class="setting"><div class="settingHeader"><label>Auto-cycle</label><label class="toggle"><input id="autoCycle" type="checkbox"><b></b></label></div></div>
          <div class="setting"><div class="settingHeader"><label>Cycle interval</label><span>seconds</span></div><input id="cycleInterval" type="number" min="1" max="86400"></div>
          <div class="setting"><div class="settingHeader"><label>Target frame rate</label><span>FPS</span></div><input id="fps" type="number" min="1" max="60"></div>
          <div class="setting"><div class="settingHeader"><label>Legacy re-sync</label><span>leave at 0 for HUB75</span></div><input id="resyncInterval" type="number" min="0" max="86400"></div>
          <button class="btn primary wide" onclick="savePlayback()">Save playback settings</button>
          <button class="btn ghost wide" onclick="command('resync')">Refresh frame timing</button>
          <button id="luaHelloButton" class="btn teal wide" onclick="toggleLuaHello()">Run Hello Lua</button>
        </div>
      </section>
      <section class="card">
        <div class="cardHead"><div><h2>Wi-Fi</h2><p>Saved networks and recovery setup</p></div></div>
        <div class="cardBody">
          <div id="networkInfo" class="networkInfo">Loading network status…</div>
          <div class="divider"></div>
          <div class="settingsGrid"><input id="newSsid" type="text" maxlength="32" placeholder="Network name (SSID)"><input id="newPass" type="password" maxlength="64" placeholder="Password"><button class="btn teal wide" onclick="addNetwork()">Save network</button><button class="btn danger wide" onclick="resetWifi()">Forget all Wi-Fi networks</button></div>
        </div>
      </section>
    </aside>
  </main>
</div>
<div id="toast" class="toast"></div>
<script>
const defs=[
 {name:'Starfield',params:[['b','Brightness',0,15,1,'RGB brightness (full range)'],['as','Animation speed',0.1,3,0.05,'Overall movement multiplier'],['n','Stars',10,300,1,'Number of active stars'],['s','Perspective scale',2,10,0.1,'Field-of-view scale'],['w','Warp speed',0.05,1,0.01,'Forward travel speed']],lfo:['Animation speed','Stars','Perspective scale','Warp speed']},
 {name:'Warped Checkerboard',params:[['b','Brightness',0,15,1,'RGB brightness (full range)'],['as','Animation speed',0.1,3,0.05,'Overall animation rate'],['ws','Wave scale',2,10,0.1,'Spacing of the radial distortion'],['d','Distortion',1,8,0.1,'Warping depth'],['wsp','Wave speed',1,8,0.1,'Distortion travel speed']],lfo:['Animation speed','Wave scale','Distortion','Wave speed']},
 {name:'Moiré Rings',params:[['b','Brightness',0,15,1,'RGB brightness (full range)'],['as','Animation speed',0.1,3,0.05,'Overall animation rate'],['orb','Orbit radius',0,16,0.1,'Movement radius of ring centers'],['rt','Ring thickness',2,16,0.1,'Ring spacing / thickness'],['es','Expansion speed',0.1,5,0.05,'Ring phase speed']],lfo:['Animation speed','Orbit radius','Ring thickness','Expansion speed']},
 {name:'Moiré Lines',params:[['b','Brightness',0,15,1,'RGB brightness (full range)'],['as','Animation speed',0.1,3,0.05,'Overall animation rate'],['rs1','Rotation one',0.1,3,0.05,'First line-field rotation'],['rs2','Rotation two',0.1,3,0.05,'Second line-field rotation'],['lf','Line frequency',0.1,2,0.05,'Density of line fields']],lfo:['Animation speed','Rotation one','Rotation two','Line frequency']},
 {name:'Warping Checkers',params:[['b','Brightness',0,15,1,'RGB brightness (full range)'],['as','Animation speed',0.1,3,0.05,'Overall animation rate'],['os','Orbit speed',0.1,2,0.05,'Motion of distortion center'],['rt','Ring thickness',1,8,0.1,'Band spacing'],['es','Expansion speed',0.1,2,0.05,'Outward phase motion']],lfo:['Animation speed','Orbit speed','Ring thickness','Expansion speed']},
 {name:'Shockwave',params:[['b','Brightness',0,15,1,'RGB brightness (full range)'],['as','Animation speed',0.1,3,0.05,'Overall animation rate'],['ws','Wave speed',0.1,5,0.05,'Radial wave travel speed'],['wt','Wave thickness',0.1,5,0.05,'Radial wavelength']],lfo:['Animation speed','Wave speed','Wave thickness']},
 {name:'Warp Corridors',params:[['b','Brightness',0,15,1,'RGB brightness (full range)'],['as','Animation speed',0.1,3,0.05,'Overall animation rate'],['rs','Rotation speed',0.1,2,0.05,'Corridor rotation'],['ws','Warp speed',0.1,2,0.05,'Frequency breathing rate'],['bf','Band frequency',0.1,2,0.05,'Corridor stripe density']],lfo:['Animation speed','Rotation speed','Warp speed','Band frequency']},
 {name:'Pixel Avalanche',params:[['b','Brightness',0,15,1,'RGB brightness (full range)'],['as','Animation speed',0.1,3,0.05,'Simulation update rate'],['sr','Spawn rate',1,10,1,'New pixels per update']],lfo:['Animation speed','Spawn rate']},
 {name:'Kaleidoscope',params:[['b','Brightness',0,15,1,'RGB brightness (full range)'],['as','Animation speed',0.1,3,0.05,'Overall animation rate'],['p','Points',1,15,1,'Number of source points'],['rs','Rotation speed',0.1,2,0.05,'Pattern rotation'],['es','Expansion speed',0.1,2,0.05,'Radius pulsing speed']],lfo:['Animation speed','Points','Rotation speed','Expansion speed']}
];
let state=null,timers={};
const $=id=>document.getElementById(id);
function toast(message,ok=true){const t=$('toast');t.textContent=message;t.className='toast show '+(ok?'ok':'bad');clearTimeout(toast._t);toast._t=setTimeout(()=>t.className='toast',2600)}
async function api(url,opt){const r=await fetch(url,opt);let d={};try{d=await r.json()}catch(e){}if(!r.ok||d.success===false)throw new Error(d.message||('HTTP '+r.status));return d}
async function post(body){return api('/control',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})}
async function command(command,extra={}){try{const d=await post({command,...extra});toast(d.message||'Updated');if(['setPattern','loadPatch','setDefault'].includes(command))await refreshState()}catch(e){toast(e.message,false)}}
function debounce(key,fn){clearTimeout(timers[key]);timers[key]=setTimeout(fn,130)}
function makeSlider(parent,p,current,anim){const [key,label,min,max,step,help]=p;const box=document.createElement('div');box.className='control';box.innerHTML=`<div class="controlTop"><label>${label}</label><span class="value" id="v_${key}">${current}</span></div><div class="rangeRow"><input id="r_${key}" type="range" min="${min}" max="${max}" step="${step}" value="${current}"><input id="n_${key}" class="miniInput" type="number" min="${min}" max="${max}" step="${step}" value="${current}"></div><p class="help">${help}</p>`;parent.appendChild(box);const range=box.querySelector('#r_'+key),num=box.querySelector('#n_'+key),val=box.querySelector('#v_'+key);const update=(raw,send=true)=>{let v=Number(raw);v=Math.max(min,Math.min(max,v));range.value=v;num.value=v;val.textContent=v;if(send)debounce(anim+'_'+key,()=>command('setParam',{animIndex:anim,paramName:key,value:v}))};range.oninput=()=>update(range.value);num.onchange=()=>update(num.value)}
function renderParams(){const d=defs[state.currentPattern],params=$('paramControls');params.innerHTML='';d.params.forEach(p=>makeSlider(params,p,state.params[p[0]],state.currentPattern));const lfo=$('lfoControls');lfo.innerHTML='';const target=document.createElement('div');target.className='control';target.innerHTML='<div class="controlTop"><label>Target</label></div><select id="lfoTarget"><option value="0">Off</option>'+d.lfo.map((x,i)=>`<option value="${i+1}">${x}</option>`).join('')+'</select><p class="help">Parameter controlled by the sine LFO</p>';lfo.appendChild(target);target.querySelector('select').value=state.params.lfo_target;target.querySelector('select').onchange=e=>command('setParam',{animIndex:state.currentPattern,paramName:'lfo_target',value:Number(e.target.value)});makeSlider(lfo,['lfo_speed','LFO speed',0.05,10,0.05,'Oscillation speed'],state.params.lfo_speed,state.currentPattern);makeSlider(lfo,['lfo_amount','LFO amount',0,1,0.01,'Modulation depth from 0 to 100%'],state.params.lfo_amount,state.currentPattern)}
function renderPatches(){const root=$('patches');root.innerHTML='';for(let i=0;i<3;i++){const p=document.createElement('div');p.className='patch '+(state.defaultPatches[state.currentPattern]===i?'isDefault':'');p.innerHTML=`<strong>Patch ${i+1}<span class="defaultMark">AUTO DEFAULT</span></strong><div class="buttonRow"><button class="btn ghost">Load</button><button class="btn primary">Save</button></div><button class="btn teal wide" style="margin-top:7px">Use for auto-cycle</button>`;const b=p.querySelectorAll('button');b[0].onclick=()=>command('loadPatch',{animIndex:state.currentPattern,patchIndex:i});b[1].onclick=()=>{if(confirm('Overwrite patch '+(i+1)+' with the current settings?'))command('savePatch',{animIndex:state.currentPattern,patchIndex:i})};b[2].onclick=()=>command('setDefault',{animIndex:state.currentPattern,patchIndex:i});root.appendChild(p)}}
function render(){const select=$('pattern');if(!select.options.length){defs.forEach((d,i)=>select.add(new Option(d.name,i)));select.onchange=e=>command('setPattern',{value:Number(e.target.value)})}select.value=state.currentPattern;$('autoCycle').checked=state.autoCycleEnabled;$('cycleInterval').value=state.cycleInterval;$('fps').value=state.targetFPS;$('resyncInterval').value=state.resyncInterval;$('modeBadge').textContent=state.networkMode==='ap'?'Setup access point':'Wi-Fi connected';$('ipBadge').textContent=state.ip;const net=state.networkMode==='ap'?`<strong>Setup mode</strong><br>Connect to <strong>${state.apSsid}</strong><br>Open <strong>${state.ip}</strong>`:`<strong>${state.ssid||'Wi-Fi'}</strong><br>IP: <strong>${state.ip}</strong><br>Signal: <strong>${state.rssi} dBm</strong><br>Address: <strong>http://${state.hostname}.local</strong>`;$('networkInfo').innerHTML=net;const luaButton=$('luaHelloButton');luaButton.disabled=!state.luaHelloAvailable;luaButton.textContent=state.luaHelloEnabled?'Stop Hello Lua':'Run Hello Lua';luaButton.title=state.luaHelloAvailable?'Lua calls the new C++ graphics backend':(state.luaError||'Lua runtime unavailable');renderParams();renderPatches()}
async function refreshState(){try{state=await api('/control');render()}catch(e){toast(e.message,false)}}
async function savePlayback(){try{await command('setAutoCycle',{value:$('autoCycle').checked});await command('setCycleInterval',{value:Number($('cycleInterval').value)});await command('setFPS',{value:Number($('fps').value)});await command('setResyncInterval',{value:Number($('resyncInterval').value)});await refreshState()}catch(e){toast(e.message,false)}}
async function toggleLuaHello(){await command(state.luaHelloEnabled?'stopLuaHello':'runLuaHello');await refreshState()}
async function addNetwork(){const ssid=$('newSsid').value.trim(),pass=$('newPass').value;if(!ssid)return toast('Enter an SSID',false);try{const d=await api('/wifi/add',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid,password:pass})});$('newSsid').value='';$('newPass').value='';toast(d.message||'Network saved')}catch(e){toast(e.message,false)}}
async function resetWifi(){if(!confirm('Forget every stored Wi-Fi network and restart in setup mode?'))return;try{await api('/wifi/reset',{method:'POST'});toast('Restarting in setup mode')}catch(e){toast(e.message,false)}}
window.addEventListener('load',refreshState);
</script>
</body>
</html>
)HTML";

const char SETUP_HTML[] PROGMEM = R"HTML(
<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="theme-color" content="#090b12"><title>Matrix Wi-Fi Setup</title><style>
:root{color-scheme:dark;--line:rgba(255,255,255,.1);--a:#7c5cff;--b:#18d5c2;--muted:#9aa3b7}*{box-sizing:border-box}body{margin:0;min-height:100vh;display:grid;place-items:center;padding:18px;font-family:Inter,system-ui,-apple-system,"Segoe UI",sans-serif;color:#f5f7ff;background:radial-gradient(circle at 15% 10%,rgba(124,92,255,.25),transparent 34%),radial-gradient(circle at 90% 10%,rgba(24,213,194,.16),transparent 30%),#080a10}.card{width:min(520px,100%);padding:28px;border:1px solid var(--line);border-radius:24px;background:rgba(20,24,36,.88);box-shadow:0 25px 90px rgba(0,0,0,.5);backdrop-filter:blur(18px)}.icon{width:56px;height:56px;border-radius:18px;background:linear-gradient(135deg,var(--a),var(--b));display:grid;place-items:center;font-size:25px;margin-bottom:18px}h1{font-size:1.55rem;margin:0 0 8px}p{color:var(--muted);line-height:1.55;margin:0 0 22px}.field{margin:12px 0}label{display:block;font-size:.78rem;color:var(--muted);margin:0 0 7px}input,select{width:100%;padding:13px 14px;border-radius:12px;border:1px solid var(--line);background:#101522;color:white;outline:none}input:focus,select:focus{border-color:var(--a);box-shadow:0 0 0 3px rgba(124,92,255,.13)}button{width:100%;padding:13px;border:0;border-radius:12px;background:linear-gradient(135deg,var(--a),#5c79ff);color:white;font-weight:800;cursor:pointer;margin-top:9px}.secondary{background:rgba(255,255,255,.06);border:1px solid var(--line)}.note{margin-top:18px;padding:13px;border:1px solid var(--line);border-radius:13px;font-size:.78rem;color:var(--muted);line-height:1.55}.msg{min-height:22px;margin-top:13px;font-size:.82rem;color:var(--b)}
</style></head><body><main class="card"><div class="icon">⌁</div><h1>Connect your matrix</h1><p>No saved Wi-Fi network was reachable, so the ESP32 opened its own setup network. Choose a network, enter its password, and the controller will restart.</p><div class="field"><label>Wi-Fi network</label><select id="ssid"><option value="">Scanning…</option></select></div><div class="field"><label>Or enter SSID manually</label><input id="manual" maxlength="32" placeholder="Network name"></div><div class="field"><label>Password</label><input id="pass" type="password" maxlength="64" placeholder="Wi-Fi password"></div><button onclick="save()">Save and restart</button><button class="secondary" onclick="scan()">Scan again</button><div id="msg" class="msg"></div><div class="note">Setup access point: <strong>64x64-Matrix-Setup</strong><br>Local setup address: <strong>192.168.4.1</strong><br>The matrix animations continue running during setup.</div></main><script>
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
  }
}

bool setAnimationParam(int animIndex, const char* name, JsonVariantConst value) {
  if (animIndex < 0 || animIndex >= NUM_ANIMATIONS || name == nullptr) return false;

  const float f = value.as<float>();
  const int i = value.as<int>();
  bool changed = true;

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
  if (isAPMode) server.send_P(200, "text/html; charset=utf-8", SETUP_HTML);
  else server.send_P(200, "text/html; charset=utf-8", DASHBOARD_HTML);
}

void handleControlGet() {
  DynamicJsonDocument doc(4096);
  doc["success"] = true;
  doc["currentPattern"] = (int)currentAnimation;
  doc["targetFPS"] = targetFPS;
  doc["autoCycleEnabled"] = autoCycleEnabled;
  doc["cycleInterval"] = autoCycleIntervalSecs;
  doc["resyncInterval"] = autoResyncInterval;
  doc["luaHelloAvailable"] = luaPattern.available();
  doc["luaHelloEnabled"] = luaHelloEnabled;
  doc["luaMemoryUsed"] = luaPattern.memoryUsed();
  doc["luaMemoryLimit"] = luaPattern.memoryLimit();
  doc["luaError"] = luaPattern.lastError();
  doc["networkMode"] = isAPMode ? "ap" : "station";
  doc["hostname"] = HOSTNAME;
  doc["apSsid"] = SETUP_AP_SSID;

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
  server.send(200, "application/json", response);
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
  }
  preferences.end();
}

void handleControlPost() {
  isUpdatingWeb = true;

  DynamicJsonDocument doc(1024);
  const DeserializationError error = deserializeJson(doc, server.arg("plain"));
  if (error) {
    isUpdatingWeb = false;
    sendJson(false, String("Invalid JSON: ") + error.c_str(), 400);
    return;
  }

  const char* commandName = doc["command"];
  if (!commandName) {
    isUpdatingWeb = false;
    sendJson(false, "Missing command", 400);
    return;
  }

  bool success = true;
  String message = "Updated";

  if (!strcmp(commandName, "setPattern")) {
    const int value = doc["value"].as<int>();
    if (value < 0 || value >= NUM_ANIMATIONS) success = false;
    else {
      luaHelloEnabled = false;
      currentAnimation = (Animation)value;
      loadPatch(currentAnimation, defaultPatches[currentAnimation]);
      Serial.print("Pattern selected: ");
      Serial.println(animationNames[currentAnimation]);
      lastAnimationChangeTime = millis();
      message = String("Pattern: ") + animationNames[currentAnimation];
    }
  } else if (!strcmp(commandName, "setAutoCycle")) {
    autoCycleEnabled = doc["value"].as<bool>();
    preferences.begin("matrix-cfg", false); preferences.putBool("auto_cycle", autoCycleEnabled); preferences.end();
    lastAnimationChangeTime = millis();
    message = autoCycleEnabled ? "Auto-cycle enabled" : "Auto-cycle disabled";
  } else if (!strcmp(commandName, "setFPS")) {
    targetFPS = clampInt(doc["value"].as<int>(), 1, 60);
    preferences.begin("matrix-cfg", false); preferences.putInt("global_fps", targetFPS); preferences.end();
    message = "Target FPS saved";
  } else if (!strcmp(commandName, "setCycleInterval")) {
    autoCycleIntervalSecs = constrain(doc["value"].as<unsigned long>(), 1UL, 86400UL);
    preferences.begin("matrix-cfg", false); preferences.putULong("cycle_int", autoCycleIntervalSecs); preferences.end();
    lastAnimationChangeTime = millis();
    message = "Cycle interval saved";
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
    if (animIndex < 0 || animIndex >= NUM_ANIMATIONS || patchIndex < 0 || patchIndex >= NUM_PATCH_SLOTS || animIndex != currentAnimation) success = false;
    else {
      savePatchToPreferences(animIndex, patchIndex);
      message = String("Saved patch ") + (patchIndex + 1);
    }
  } else if (!strcmp(commandName, "loadPatch")) {
    const int animIndex = doc["animIndex"].as<int>();
    const int patchIndex = doc["patchIndex"].as<int>();
    if (animIndex < 0 || animIndex >= NUM_ANIMATIONS || patchIndex < 0 || patchIndex >= NUM_PATCH_SLOTS) success = false;
    else {
      luaHelloEnabled = false;
      currentAnimation = (Animation)animIndex;
      loadPatch(currentAnimation, patchIndex);
      lastAnimationChangeTime = millis();
      message = String("Loaded patch ") + (patchIndex + 1);
    }
  } else if (!strcmp(commandName, "setDefault")) {
    const int animIndex = doc["animIndex"].as<int>();
    const int patchIndex = doc["patchIndex"].as<int>();
    if (animIndex < 0 || animIndex >= NUM_ANIMATIONS || patchIndex < 0 || patchIndex >= NUM_PATCH_SLOTS) success = false;
    else {
      defaultPatches[animIndex] = patchIndex;
      preferences.begin("matrix-cfg", false); preferences.putBytes("def_p", defaultPatches, sizeof(defaultPatches)); preferences.end();
      message = String("Patch ") + (patchIndex + 1) + " is the auto-cycle default";
    }
  } else if (!strcmp(commandName, "runLuaHello")) {
    if (!luaPattern.available()) {
      success = false;
      message = String("Lua unavailable: ") + luaPattern.lastError();
    } else {
      luaHelloEnabled = true;
      animationTime = 0;
      lastAnimationChangeTime = millis();
      message = "Hello Lua started";
    }
  } else if (!strcmp(commandName, "stopLuaHello")) {
    luaHelloEnabled = false;
    lastAnimationChangeTime = millis();
    message = "Hello Lua stopped";
  } else if (!strcmp(commandName, "resync")) {
    resyncDisplay();
    message = "Frame timing refreshed";
  } else {
    success = false;
    message = "Unknown command";
  }

  isUpdatingWeb = false;
  sendJson(success, message, success ? 200 : 400);
}

void handleWifiScan() {
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
  server.send(found >= 0 ? 200 : 500, "application/json", response);
}

void handleWifiAdd() {
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

  sendJson(true, restartAfterSave ? "Network saved. Restarting…" : "Network saved. It will be tried after the next restart.");

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
  server.on("/control", HTTP_GET, handleControlGet);
  server.on("/control", HTTP_POST, handleControlPost);

  server.on("/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/wifi/add", HTTP_POST, handleWifiAdd);
  server.on("/wifi/reset", HTTP_POST, handleWifiReset);

  // Compatibility aliases for the routes used in the second sketch.
  server.on("/addNetwork", HTTP_POST, handleWifiAdd);
  server.on("/resetWifi", HTTP_POST, handleWifiReset);
  server.on("/save", HTTP_POST, handleSetupSave);

  // Common captive-portal probes.
  server.on("/generate_204", HTTP_ANY, handleRoot);
  server.on("/gen_204", HTTP_ANY, handleRoot);
  server.on("/hotspot-detect.html", HTTP_ANY, handleRoot);
  server.on("/library/test/success.html", HTTP_ANY, handleRoot);
  server.on("/ncsi.txt", HTTP_ANY, handleRoot);
  server.on("/connecttest.txt", HTTP_ANY, handleRoot);
  server.on("/redirect", HTTP_ANY, handleRoot);
  server.on("/fwlink", HTTP_ANY, handleRoot);

  server.onNotFound(handleNotFound);
}
