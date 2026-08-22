#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../platform/DisplayBackend.h"

struct lua_State;
struct lua_Debug;

class LuaPatternRuntime {
 public:
  explicit LuaPatternRuntime(DisplayBackend& display);
  ~LuaPatternRuntime();

  bool loadFromString(const char* source, const char* chunkName);
  bool frame(float timeSeconds, float deltaSeconds);
  void shutdown();

  bool available() const { return state_ != nullptr && frameRef_ >= 0; }
  const char* lastError() const { return lastError_; }
  size_t memoryUsed() const { return allocator_.used; }
  size_t memoryLimit() const { return allocator_.limit; }

 private:
  struct AllocatorState {
    size_t used;
    size_t limit;
  };

  static constexpr size_t kMemoryLimitBytes = 96 * 1024;
  static constexpr int kHookInterval = 1000;
  static constexpr int kFrameInstructionBudget = 50000;
  static constexpr uint32_t kFrameTimeBudgetMicros = 12000;

  static void* limitedAllocator(void* userData, void* pointer,
                                size_t oldSize, size_t newSize);
  static void instructionHook(lua_State* state, lua_Debug* debugInfo);
  static LuaPatternRuntime* runtimeFromUpvalue(lua_State* state);
  static LuaPatternRuntime* runtimeFromState(lua_State* state);

  static int luaGfxClear(lua_State* state);
  static int luaGfxPixel(lua_State* state);
  static int luaGfxLine(lua_State* state);
  static int luaGfxRect(lua_State* state);
  static int luaGfxFillRect(lua_State* state);
  static int luaGfxCircle(lua_State* state);
  static int luaColorRgb(lua_State* state);
  static int luaColorHsv(lua_State* state);

  void openSafeLibraries();
  void registerGraphicsApi();
  void createContext();
  void updateContext(float timeSeconds, float deltaSeconds);
  void startBudget(int instructionBudget, uint32_t timeBudgetMicros);
  bool callReference(int reference, int argumentCount);
  void captureError(const char* prefix);

  DisplayBackend& display_;
  AllocatorState allocator_;
  lua_State* state_;
  int initRef_;
  int frameRef_;
  int contextRef_;
  int remainingHookTicks_;
  uint32_t deadlineMicros_;
  char lastError_[192];
};
