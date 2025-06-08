//========================================================================
//
// runtime_random.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "runtime_random.h"

//------------------------------------------------------------------------

#define M1 259200
#define IA1 7141
#define IC1 54773
#define RM1 (1.0 / M1)

#define M2 134456
#define IA2 8121
#define IC2 28411
#define RM2 (1.0 / M2)

#define M3 243000
#define IA3 4561
#define IC3 51349

struct Random {
  int ix1, ix2, ix3;
  double r[97];
};

static Random globalRandom;

//------------------------------------------------------------------------

static void seedrand(int64_t seed) {
  // seed the first generator
  globalRandom.ix1 = IC1 + (int)seed;
  if (globalRandom.ix1 < 0) {
    globalRandom.ix1 = -globalRandom.ix1;
  }
  globalRandom.ix1 %= M1;

  // use the first generator to seed the second
  globalRandom.ix1 = (IA1 * globalRandom.ix1 + IC1) % M1;
  globalRandom.ix2 = globalRandom.ix1 % M2;

  // use the first generator to seed the third
  globalRandom.ix1 = (IA1 * globalRandom.ix1 + IC1) % M1;
  globalRandom.ix3 = globalRandom.ix1 % M3;

  // fill the table
  for (int j = 0; j < 97; ++j) {
    globalRandom.ix1 = (IA1 * globalRandom.ix1 + IC1) % M1;
    globalRandom.ix2 = (IA2 * globalRandom.ix2 + IC2) % M2;
    globalRandom.r[j] = ((double)globalRandom.ix1 + (double)globalRandom.ix2 * RM2) * RM1;
  }
}

static double getrand() {
  // generate the next number in each sequence
  globalRandom.ix1 = (IA1 * globalRandom.ix1 + IC1) % M1;
  globalRandom.ix2 = (IA2 * globalRandom.ix2 + IC2) % M2;
  globalRandom.ix3 = (IA3 * globalRandom.ix3 + IC3) % M3;

  // use the third sequence to choose a table entry
  int j = (97 * globalRandom.ix3) / M3;
  double result = globalRandom.r[j];

  // replace the table entry
  globalRandom.r[j] = ((double)globalRandom.ix1 + (double)globalRandom.ix2 * RM2) * RM1;

  return result;
}

//------------------------------------------------------------------------

// seedrand(seed: Int)
static NativeFuncDefn(runtime_seedrand_I) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsInt(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &seedCell = engine.arg(0);
  seedrand(cellInt(seedCell));
  engine.push(cellMakeInt(0));
}

// rand() -> Float
static NativeFuncDefn(runtime_rand) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 0) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  engine.push(cellMakeFloat(getrand()));
}

// randi(min: Int, max: Int) -> Int
static NativeFuncDefn(runtime_randi_II) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsInt(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &minCell = engine.arg(0);
  Cell &maxCell = engine.arg(1);
  double x = getrand();
  int64_t min = cellInt(minCell);
  int64_t max = cellInt(maxCell);
  int r = min + (int)(x * (max - min));
  engine.push(cellMakeInt(r));
}

//------------------------------------------------------------------------

void runtime_random_init(BytecodeEngine &engine) {
  seedrand(123);
  engine.addNativeFunction("seedrand_I", &runtime_seedrand_I);
  engine.addNativeFunction("rand", &runtime_rand);
  engine.addNativeFunction("randi_II", &runtime_randi_II);
}
