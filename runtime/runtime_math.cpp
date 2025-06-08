//========================================================================
//
// runtime_math.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "runtime_math.h"
#include <math.h>
#include "BytecodeDefs.h"

//------------------------------------------------------------------------

// toFloat(x: Int) -> Float
static NativeFuncDefn(runtime_toFloat_I) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsInt(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  engine.push(cellMakeFloat((float)cellInt(xCell)));
}

// ceil(x: Float) -> Float
static NativeFuncDefn(runtime_ceil_F) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsFloat(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  engine.push(cellMakeFloat(ceilf(cellFloat(xCell))));
}

// floor(x: Float) -> Float
static NativeFuncDefn(runtime_floor_F) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsFloat(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  engine.push(cellMakeFloat(floorf(cellFloat(xCell))));
}

// round(x: Float) -> Float
static NativeFuncDefn(runtime_round_F) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsFloat(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  engine.push(cellMakeFloat(roundf(cellFloat(xCell))));
}

// ceili(x: Float) -> Int
static NativeFuncDefn(runtime_ceili_F) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsFloat(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  float t = ceilf(cellFloat(xCell));
  if (t > (float)bytecodeMaxInt || t < (float)bytecodeMinInt) {
    BytecodeEngine::fatalError("Integer overflow");
  }
  engine.push(cellMakeInt((int64_t)t));
}

// floori(x: Float) -> Int
static NativeFuncDefn(runtime_floori_F) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsFloat(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  float t = floorf(cellFloat(xCell));
  if (t > (float)bytecodeMaxInt || t < (float)bytecodeMinInt) {
    BytecodeEngine::fatalError("Integer overflow");
  }
  engine.push(cellMakeInt((int64_t)t));
}

// roundi(x: Float) -> Int
static NativeFuncDefn(runtime_roundi_F) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsFloat(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  float t = roundf(cellFloat(xCell));
  if (t > (float)bytecodeMaxInt || t < (float)bytecodeMinInt) {
    BytecodeEngine::fatalError("Integer overflow");
  }
  engine.push(cellMakeInt((int64_t)t));
}

// min(x: Int, y: Int) -> Int
static NativeFuncDefn(runtime_min_II) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsInt(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  Cell &yCell = engine.arg(1);
  engine.push(cellMakeInt(std::min(cellInt(xCell), cellInt(yCell))));
}

// min(x: Float, y: Float) -> Float
static NativeFuncDefn(runtime_min_FF) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsFloat(engine.arg(0)) ||
      !cellIsFloat(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  Cell &yCell = engine.arg(1);
  engine.push(cellMakeFloat(std::min(cellFloat(xCell), cellFloat(yCell))));
}

// max(x: Int, y: Int) -> Int
static NativeFuncDefn(runtime_max_II) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsInt(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  Cell &yCell = engine.arg(1);
  engine.push(cellMakeInt(std::max(cellInt(xCell), cellInt(yCell))));
}

// max(x: Float, y: Float) -> Float
static NativeFuncDefn(runtime_max_FF) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsFloat(engine.arg(0)) ||
      !cellIsFloat(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  Cell &yCell = engine.arg(1);
  engine.push(cellMakeFloat(std::max(cellFloat(xCell), cellFloat(yCell))));
}

// abs(x: Int) -> Int
static NativeFuncDefn(runtime_abs_I) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsInt(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  engine.push(cellMakeInt(labs(cellInt(xCell))));
}

// abs(x: Float) -> Float
static NativeFuncDefn(runtime_abs_F) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsFloat(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  engine.push(cellMakeFloat(fabsf(cellFloat(xCell))));
}

// sqrt(x: Float) -> Float
static NativeFuncDefn(runtime_sqrt_F) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsFloat(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  engine.push(cellMakeFloat(sqrtf(cellFloat(xCell))));
}

// pow(x: Float, y: Float) -> Float
static NativeFuncDefn(runtime_pow_FF) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsFloat(engine.arg(0)) ||
      !cellIsFloat(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  Cell &yCell = engine.arg(1);
  engine.push(cellMakeFloat(powf(cellFloat(xCell), cellFloat(yCell))));
}

// exp(x: Float) -> Float
static NativeFuncDefn(runtime_exp_F) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsFloat(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  engine.push(cellMakeFloat(expf(cellFloat(xCell))));
}

// log(x: Float) -> Float
static NativeFuncDefn(runtime_log_F) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsFloat(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  engine.push(cellMakeFloat(logf(cellFloat(xCell))));
}

// log10(x: Float) -> Float
static NativeFuncDefn(runtime_log10_F) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsFloat(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  engine.push(cellMakeFloat(log10f(cellFloat(xCell))));
}

// sin(x: Float) -> Float
static NativeFuncDefn(runtime_sin_F) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsFloat(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  engine.push(cellMakeFloat(sinf(cellFloat(xCell))));
}

// cos(x: Float) -> Float
static NativeFuncDefn(runtime_cos_F) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsFloat(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  engine.push(cellMakeFloat(cosf(cellFloat(xCell))));
}

// tan(x: Float) -> Float
static NativeFuncDefn(runtime_tan_F) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsFloat(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  engine.push(cellMakeFloat(tanf(cellFloat(xCell))));
}

// asin(x: Float) -> Float
static NativeFuncDefn(runtime_asin_F) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsFloat(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  engine.push(cellMakeFloat(asinf(cellFloat(xCell))));
}

// acos(x: Float) -> Float
static NativeFuncDefn(runtime_acos_F) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsFloat(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &xCell = engine.arg(0);
  engine.push(cellMakeFloat(acosf(cellFloat(xCell))));
}

// atan2(y:Float, x: Float) -> Float
static NativeFuncDefn(runtime_atan2_FF) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsFloat(engine.arg(0)) ||
      !cellIsFloat(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &yCell = engine.arg(0);
  Cell &xCell = engine.arg(1);
  engine.push(cellMakeFloat(atan2f(cellFloat(yCell), cellFloat(xCell))));
}

//------------------------------------------------------------------------

void runtime_math_init(BytecodeEngine &engine) {
  engine.addNativeFunction("toFloat_I", &runtime_toFloat_I);
  engine.addNativeFunction("ceil_F", &runtime_ceil_F);
  engine.addNativeFunction("floor_F", &runtime_floor_F);
  engine.addNativeFunction("round_F", &runtime_round_F);
  engine.addNativeFunction("ceili_F", &runtime_ceili_F);
  engine.addNativeFunction("floori_F", &runtime_floori_F);
  engine.addNativeFunction("roundi_F", &runtime_roundi_F);
  engine.addNativeFunction("min_II", &runtime_min_II);
  engine.addNativeFunction("min_FF", &runtime_min_FF);
  engine.addNativeFunction("max_II", &runtime_max_II);
  engine.addNativeFunction("max_FF", &runtime_max_FF);
  engine.addNativeFunction("abs_I", &runtime_abs_I);
  engine.addNativeFunction("abs_F", &runtime_abs_F);
  engine.addNativeFunction("sqrt_F", &runtime_sqrt_F);
  engine.addNativeFunction("pow_FF", &runtime_pow_FF);
  engine.addNativeFunction("exp_F", &runtime_exp_F);
  engine.addNativeFunction("log_F", &runtime_log_F);
  engine.addNativeFunction("log10_F", &runtime_log10_F);
  engine.addNativeFunction("sin_F", &runtime_sin_F);
  engine.addNativeFunction("cos_F", &runtime_cos_F);
  engine.addNativeFunction("tan_F", &runtime_tan_F);
  engine.addNativeFunction("asin_F", &runtime_asin_F);
  engine.addNativeFunction("acos_F", &runtime_acos_F);
  engine.addNativeFunction("atan2_FF", &runtime_atan2_FF);
}
