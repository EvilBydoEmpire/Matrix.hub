#include "LuaPatternRuntime.h"

#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace {
constexpr int kNoReference = -1;

uint8_t clampByte(lua_Integer value) {
  if (value < 0) return 0;
  if (value > 255) return 255;
  return static_cast<uint8_t>(value);
}

double clampUnit(lua_Number value) {
  if (!isfinite(value) || value < 0.0) return 0.0;
  if (value > 1.0) return 1.0;
  return value;
}

uint32_t packRgb(uint8_t red, uint8_t green, uint8_t blue) {
  return (static_cast<uint32_t>(red) << 16) |
         (static_cast<uint32_t>(green) << 8) | blue;
}
}  // namespace

LuaPatternRuntime::LuaPatternRuntime(DisplayBackend& display)
    : display_(display),
      allocator_{0, kMemoryLimitBytes},
      state_(nullptr),
      initRef_(kNoReference),
      frameRef_(kNoReference),
      contextRef_(kNoReference),
      remainingHookTicks_(0),
      deadlineMicros_(0),
      lastError_{} {}

LuaPatternRuntime::~LuaPatternRuntime() { shutdown(); }

void* LuaPatternRuntime::limitedAllocator(void* userData, void* pointer,
                                          size_t oldSize, size_t newSize) {
  auto* allocator = static_cast<AllocatorState*>(userData);
  const size_t accountedOldSize = pointer == nullptr ? 0 : oldSize;

  if (newSize == 0) {
    free(pointer);
    allocator->used = accountedOldSize > allocator->used
                          ? 0
                          : allocator->used - accountedOldSize;
    return nullptr;
  }

  if (newSize > accountedOldSize &&
      newSize - accountedOldSize > allocator->limit - allocator->used) {
    return nullptr;
  }

  void* resized = realloc(pointer, newSize);
  if (resized == nullptr) return nullptr;

  if (newSize >= accountedOldSize) {
    allocator->used += newSize - accountedOldSize;
  } else {
    allocator->used -= accountedOldSize - newSize;
  }
  return resized;
}

void LuaPatternRuntime::instructionHook(lua_State* state, lua_Debug*) {
  LuaPatternRuntime* runtime = runtimeFromState(state);
  if (runtime == nullptr) luaL_error(state, "missing runtime context");

  runtime->remainingHookTicks_--;
  const bool instructionLimitReached = runtime->remainingHookTicks_ <= 0;
  const bool timeLimitReached =
      static_cast<int32_t>(micros() - runtime->deadlineMicros_) >= 0;
  if (instructionLimitReached || timeLimitReached) {
    luaL_error(state, instructionLimitReached
                          ? "pattern instruction budget exceeded"
                          : "pattern frame-time budget exceeded");
  }
}

LuaPatternRuntime* LuaPatternRuntime::runtimeFromUpvalue(lua_State* state) {
  return static_cast<LuaPatternRuntime*>(
      lua_touserdata(state, lua_upvalueindex(1)));
}

LuaPatternRuntime* LuaPatternRuntime::runtimeFromState(lua_State* state) {
  return *static_cast<LuaPatternRuntime**>(lua_getextraspace(state));
}

bool LuaPatternRuntime::loadFromString(const char* source,
                                       const char* chunkName) {
  shutdown();
  allocator_ = {0, kMemoryLimitBytes};
  lastError_[0] = '\0';

  state_ = lua_newstate(limitedAllocator, &allocator_);
  if (state_ == nullptr) {
    snprintf(lastError_, sizeof(lastError_), "could not allocate Lua state");
    return false;
  }

  *static_cast<LuaPatternRuntime**>(lua_getextraspace(state_)) = this;
  openSafeLibraries();
  registerGraphicsApi();

  if (luaL_loadbuffer(state_, source, strlen(source), chunkName) != LUA_OK) {
    captureError("compile");
    shutdown();
    return false;
  }

  startBudget(100000, 50000);
  if (lua_pcall(state_, 0, 1, 0) != LUA_OK) {
    captureError("load");
    shutdown();
    return false;
  }

  if (!lua_istable(state_, -1)) {
    snprintf(lastError_, sizeof(lastError_),
             "pattern must return a table");
    lua_pop(state_, 1);
    shutdown();
    return false;
  }

  lua_getfield(state_, -1, "init");
  if (lua_isfunction(state_, -1)) {
    initRef_ = luaL_ref(state_, LUA_REGISTRYINDEX);
  } else {
    lua_pop(state_, 1);
  }

  lua_getfield(state_, -1, "frame");
  if (!lua_isfunction(state_, -1)) {
    lua_pop(state_, 2);
    snprintf(lastError_, sizeof(lastError_),
             "pattern must provide frame(ctx)");
    shutdown();
    return false;
  }
  frameRef_ = luaL_ref(state_, LUA_REGISTRYINDEX);
  lua_pop(state_, 1);  // pattern table

  createContext();
  if (initRef_ >= 0) {
    updateContext(0.0f, 0.0f);
    lua_rawgeti(state_, LUA_REGISTRYINDEX, initRef_);
    lua_rawgeti(state_, LUA_REGISTRYINDEX, contextRef_);
    startBudget(kFrameInstructionBudget, kFrameTimeBudgetMicros);
    if (!callReference(kNoReference, 1)) {
      shutdown();
      return false;
    }
  }

  lua_sethook(state_, nullptr, 0, 0);
  return true;
}

bool LuaPatternRuntime::frame(float timeSeconds, float deltaSeconds) {
  if (!available()) return false;

  updateContext(timeSeconds, deltaSeconds);
  lua_rawgeti(state_, LUA_REGISTRYINDEX, frameRef_);
  lua_rawgeti(state_, LUA_REGISTRYINDEX, contextRef_);
  startBudget(kFrameInstructionBudget, kFrameTimeBudgetMicros);
  const bool success = callReference(kNoReference, 1);
  lua_sethook(state_, nullptr, 0, 0);
  return success;
}

void LuaPatternRuntime::shutdown() {
  if (state_ != nullptr) lua_close(state_);
  state_ = nullptr;
  initRef_ = kNoReference;
  frameRef_ = kNoReference;
  contextRef_ = kNoReference;
  remainingHookTicks_ = 0;
  deadlineMicros_ = 0;
  allocator_.used = 0;
}

void LuaPatternRuntime::openSafeLibraries() {
  luaL_requiref(state_, "_G", luaopen_base, 1);
  lua_pop(state_, 1);
  luaL_requiref(state_, LUA_MATHLIBNAME, luaopen_math, 1);
  lua_pop(state_, 1);
  luaL_requiref(state_, LUA_TABLIBNAME, luaopen_table, 1);
  lua_pop(state_, 1);
  luaL_requiref(state_, LUA_STRLIBNAME, luaopen_string, 1);
  lua_pop(state_, 1);
  luaL_requiref(state_, LUA_UTF8LIBNAME, luaopen_utf8, 1);
  lua_pop(state_, 1);

  const char* blockedGlobals[] = {
      "collectgarbage", "dofile", "load", "loadfile", "print"};
  for (const char* name : blockedGlobals) {
    lua_pushnil(state_);
    lua_setglobal(state_, name);
  }
}

void LuaPatternRuntime::registerGraphicsApi() {
  struct FunctionBinding {
    const char* name;
    lua_CFunction function;
  };

  const FunctionBinding gfxFunctions[] = {
      {"clear", luaGfxClear},       {"pixel", luaGfxPixel},
      {"line", luaGfxLine},         {"rect", luaGfxRect},
      {"fillRect", luaGfxFillRect}, {"circle", luaGfxCircle},
  };
  lua_newtable(state_);
  for (const auto& binding : gfxFunctions) {
    lua_pushlightuserdata(state_, this);
    lua_pushcclosure(state_, binding.function, 1);
    lua_setfield(state_, -2, binding.name);
  }
  lua_setglobal(state_, "gfx");

  const FunctionBinding colorFunctions[] = {
      {"rgb", luaColorRgb}, {"hsv", luaColorHsv}};
  lua_newtable(state_);
  for (const auto& binding : colorFunctions) {
    lua_pushlightuserdata(state_, this);
    lua_pushcclosure(state_, binding.function, 1);
    lua_setfield(state_, -2, binding.name);
  }
  lua_setglobal(state_, "color");
}

void LuaPatternRuntime::createContext() {
  lua_newtable(state_);
  lua_pushinteger(state_, display_.width());
  lua_setfield(state_, -2, "width");
  lua_pushinteger(state_, display_.height());
  lua_setfield(state_, -2, "height");
  lua_pushnumber(state_, static_cast<lua_Number>(display_.width()) /
                             display_.height());
  lua_setfield(state_, -2, "aspect");
  contextRef_ = luaL_ref(state_, LUA_REGISTRYINDEX);
}

void LuaPatternRuntime::updateContext(float timeSeconds, float deltaSeconds) {
  lua_rawgeti(state_, LUA_REGISTRYINDEX, contextRef_);
  lua_pushnumber(state_, timeSeconds);
  lua_setfield(state_, -2, "time");
  lua_pushnumber(state_, deltaSeconds);
  lua_setfield(state_, -2, "deltaTime");
  lua_pop(state_, 1);
}

void LuaPatternRuntime::startBudget(int instructionBudget,
                                    uint32_t timeBudgetMicros) {
  remainingHookTicks_ =
      (instructionBudget + kHookInterval - 1) / kHookInterval;
  deadlineMicros_ = micros() + timeBudgetMicros;
  lua_sethook(state_, instructionHook, LUA_MASKCOUNT, kHookInterval);
}

bool LuaPatternRuntime::callReference(int, int argumentCount) {
  if (lua_pcall(state_, argumentCount, 0, 0) == LUA_OK) return true;
  captureError("runtime");
  return false;
}

void LuaPatternRuntime::captureError(const char* prefix) {
  const char* message = lua_tostring(state_, -1);
  snprintf(lastError_, sizeof(lastError_), "%s: %s", prefix,
           message != nullptr ? message : "unknown Lua error");
  lua_pop(state_, 1);
}

int LuaPatternRuntime::luaGfxClear(lua_State* state) {
  LuaPatternRuntime* runtime = runtimeFromUpvalue(state);
  const uint32_t color = static_cast<uint32_t>(luaL_optinteger(state, 1, 0));
  runtime->display_.clear(color);
  return 0;
}

int LuaPatternRuntime::luaGfxPixel(lua_State* state) {
  LuaPatternRuntime* runtime = runtimeFromUpvalue(state);
  runtime->display_.setPixel(luaL_checkinteger(state, 1),
                             luaL_checkinteger(state, 2),
                             luaL_checkinteger(state, 3));
  return 0;
}

int LuaPatternRuntime::luaGfxLine(lua_State* state) {
  LuaPatternRuntime* runtime = runtimeFromUpvalue(state);
  runtime->display_.drawLine(
      luaL_checkinteger(state, 1), luaL_checkinteger(state, 2),
      luaL_checkinteger(state, 3), luaL_checkinteger(state, 4),
      luaL_checkinteger(state, 5));
  return 0;
}

int LuaPatternRuntime::luaGfxRect(lua_State* state) {
  LuaPatternRuntime* runtime = runtimeFromUpvalue(state);
  runtime->display_.drawRect(
      luaL_checkinteger(state, 1), luaL_checkinteger(state, 2),
      luaL_checkinteger(state, 3), luaL_checkinteger(state, 4),
      luaL_checkinteger(state, 5));
  return 0;
}

int LuaPatternRuntime::luaGfxFillRect(lua_State* state) {
  LuaPatternRuntime* runtime = runtimeFromUpvalue(state);
  runtime->display_.fillRect(
      luaL_checkinteger(state, 1), luaL_checkinteger(state, 2),
      luaL_checkinteger(state, 3), luaL_checkinteger(state, 4),
      luaL_checkinteger(state, 5));
  return 0;
}

int LuaPatternRuntime::luaGfxCircle(lua_State* state) {
  LuaPatternRuntime* runtime = runtimeFromUpvalue(state);
  runtime->display_.drawCircle(
      luaL_checkinteger(state, 1), luaL_checkinteger(state, 2),
      luaL_checkinteger(state, 3), luaL_checkinteger(state, 4));
  return 0;
}

int LuaPatternRuntime::luaColorRgb(lua_State* state) {
  const uint8_t red = clampByte(luaL_checkinteger(state, 1));
  const uint8_t green = clampByte(luaL_checkinteger(state, 2));
  const uint8_t blue = clampByte(luaL_checkinteger(state, 3));
  lua_pushinteger(state, packRgb(red, green, blue));
  return 1;
}

int LuaPatternRuntime::luaColorHsv(lua_State* state) {
  double hue = luaL_checknumber(state, 1);
  const double saturation = clampUnit(luaL_checknumber(state, 2));
  const double value = clampUnit(luaL_checknumber(state, 3));
  hue -= floor(hue);

  const double scaledHue = hue * 6.0;
  const int sector = static_cast<int>(floor(scaledHue)) % 6;
  const double fraction = scaledHue - floor(scaledHue);
  const double p = value * (1.0 - saturation);
  const double q = value * (1.0 - saturation * fraction);
  const double t = value * (1.0 - saturation * (1.0 - fraction));

  double red = value;
  double green = t;
  double blue = p;
  switch (sector) {
    case 1: red = q; green = value; blue = p; break;
    case 2: red = p; green = value; blue = t; break;
    case 3: red = p; green = q; blue = value; break;
    case 4: red = t; green = p; blue = value; break;
    case 5: red = value; green = p; blue = q; break;
  }

  lua_pushinteger(state,
                  packRgb(static_cast<uint8_t>(round(red * 255.0)),
                          static_cast<uint8_t>(round(green * 255.0)),
                          static_cast<uint8_t>(round(blue * 255.0))));
  return 1;
}
