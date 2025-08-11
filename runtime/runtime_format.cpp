//========================================================================
//
// runtime_format.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "runtime_format.h"
#include <string.h>
#include "NumConversion.h"
#include "UTF8.h"
#include "runtime_String.h"

// Create a string on the heap, using [length] bytes of [s].  If
// [width] is non-negative, [s] is right-justified (with space chars)
// in a field of [width]. If [width] is negative, [s] is
// left-justified in a field of -[width]. The resulting string is
// pushed onto the stack.
static void formatWidth(const char *s, size_t length, int64_t width, BytecodeEngine &engine) {
  bool left;
  int64_t ww;
  if (width < 0) {
    left = true;
    ww = -width;
  } else {
    left = false;
    ww = width;
  }
  if (length >= ww) {
    ww = length;
  }
  // NB: this may trigger GC.
  Cell outCell = stringAlloc(ww, engine);
  engine.push(outCell);
  uint8_t *outData = stringData(outCell);
  if (length == ww) {
    memcpy(outData, s, length);
  } else if (left) {
    memcpy(outData, s, length);
    memset(outData + length, 0x20, ww - length);
  } else {
    memset(outData, 0x20, ww - length);
    memcpy(outData + ww - length, s, length);
  }
}

// format(x: Int, width: Int, precision: Int, format: Int) -> String
static NativeFuncDefn(runtime_format_IIII) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 4 ||
      !cellIsInt(engine.arg(0)) ||
      !cellIsInt(engine.arg(1)) ||
      !cellIsInt(engine.arg(2)) ||
      !cellIsInt(engine.arg(3))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  Cell &widthCell = engine.arg(1);
  Cell &precisionCell = engine.arg(2);
  Cell &formatCell = engine.arg(3);
  int64_t x = cellInt(xCell);
  int64_t width = cellInt(widthCell);
  int64_t precision = cellInt(precisionCell);
  int64_t format = cellInt(formatCell);

  std::string s;
  if (format == 'c') {
    uint8_t u[utf8MaxBytes];
    int uLen = utf8Encode((uint32_t)x, u);
    if (uLen > 0) {
      s = std::string((char *)u, uLen);
    }
  } else {
    int radix;
    if (format == 'b') {
      radix = 2;
    } else if (format == 'o') {
      radix = 8;
    } else if (format == 'x') {
      radix = 16;
    } else {
      radix = 10;
    }
    s = int56ToString(x, radix, (int)precision);
  }
  formatWidth(s.c_str(), s.size(), width, engine);
}

// format(x: Float, width: Int, precision: Int, format: Int) -> String
static NativeFuncDefn(runtime_format_FIII) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 4 ||
      !cellIsFloat(engine.arg(0)) ||
      !cellIsInt(engine.arg(1)) ||
      !cellIsInt(engine.arg(2)) ||
      !cellIsInt(engine.arg(3))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  Cell &widthCell = engine.arg(1);
  Cell &precisionCell = engine.arg(2);
  Cell &formatCell = engine.arg(3);
  float x = cellFloat(xCell);
  int64_t width = cellInt(widthCell);
  int64_t precision = cellInt(precisionCell);
  int64_t format = cellInt(formatCell);

  std::string s = floatToString(x, (uint8_t)format, (int)precision);
  formatWidth(s.c_str(), s.size(), width, engine);
}

// format(x: Bool, width: Int, precision: Int, format: Int) -> String
static NativeFuncDefn(runtime_format_BIII) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 4 ||
      !cellIsBool(engine.arg(0)) ||
      !cellIsInt(engine.arg(1)) ||
      !cellIsInt(engine.arg(2)) ||
      !cellIsInt(engine.arg(3))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  Cell &widthCell = engine.arg(1);
  Cell &precisionCell = engine.arg(2);
  Cell &formatCell = engine.arg(3);
  bool x = cellBool(xCell);
  int64_t width = cellInt(widthCell);

  if (x) {
    formatWidth("true", 4, width, engine);
  } else {
    formatWidth("false", 5, width, engine);
  }
}

// format(x: String, width: Int, precision: Int, format: Int) -> String
static NativeFuncDefn(runtime_format_SIII) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 4 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1)) ||
      !cellIsInt(engine.arg(2)) ||
      !cellIsInt(engine.arg(3))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &x = engine.arg(0);
  Cell &widthCell = engine.arg(1);
  Cell &precisionCell = engine.arg(2);
  Cell &formatCell = engine.arg(3);
  int64_t width = cellInt(widthCell);
  int64_t precision = cellInt(precisionCell);
  int64_t format = cellInt(formatCell);

  int64_t n = stringByteLength(x);
  if (precision >= 0 && n > precision) {
    n = precision;
  }
  // copy the string to a std::string here, because formatWidth can
  // trigger GC
  std::string s = stringToStdString(x);
  formatWidth(s.c_str(), n, width, engine);
}

void runtime_format_init(BytecodeEngine &engine) {
  engine.addNativeFunction("format_IIII", &runtime_format_IIII);
  engine.addNativeFunction("format_FIII", &runtime_format_FIII);
  engine.addNativeFunction("format_BIII", &runtime_format_BIII);
  engine.addNativeFunction("format_SIII", &runtime_format_SIII);
}
