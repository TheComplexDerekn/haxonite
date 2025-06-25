//========================================================================
//
// runtime_String.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "runtime_String.h"
#include <string.h>
#include "BytecodeDefs.h"
#include "NumConversion.h"
#include "UTF8.h"

// length(s: String) -> Int
static NativeFuncDefn(runtime_length_S) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &s = engine.arg(0);

  engine.push(cellMakeInt(stringLength(s)));
}

// get(s: String, idx: Int) -> Int
// iget(s: String, iter: Int) -> Int
static NativeFuncDefn(runtime_get_SI) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &s = engine.arg(0);
  Cell &idxCell = engine.arg(1);

  int64_t idx = cellInt(idxCell);
  int64_t length = stringLength(s);
  if (idx < 0 || idx >= length) {
    BytecodeEngine::fatalError("Index out of bounds");
  }
  uint32_t u;
  if (!utf8Get(stringData(s), length, idx, u)) {
    BytecodeEngine::fatalError("Index out of bounds");
  }
  engine.push(cellMakeInt((int64_t)u));
}

// next(s: String, idx: Int) -> Int
// inext(s: String, iter: Int) -> Int
static NativeFuncDefn(runtime_next_SI) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &s = engine.arg(0);
  Cell &iterCell = engine.arg(1);

  int64_t iter = cellInt(iterCell);
  int64_t length = stringLength(s);
  if (iter < 0 || iter >= length) {
    BytecodeEngine::fatalError("Index out of bounds");
  }
  int n = utf8Length(stringData(s), length, iter);
  engine.push(cellMakeInt(iter + n));
}

// byte(s: String, idx: Int) -> Int
static NativeFuncDefn(runtime_byte_SI) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &s = engine.arg(0);
  Cell &idxCell = engine.arg(1);

  int64_t idx = cellInt(idxCell);
  int64_t length = stringLength(s);
  if (idx < 0 || idx >= length) {
    BytecodeEngine::fatalError("Index out of bounds");
  }
  uint8_t *sData = (uint8_t *)stringData(s);

  engine.push(cellMakeInt(sData[idx]));
}

// compare(s1: String, s2: String) -> Int
static NativeFuncDefn(runtime_compare_SS) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &s1 = engine.arg(0);
  Cell &s2 = engine.arg(1);

  int64_t cmp = stringCompare(s1, s2);
  engine.push(cellMakeInt(cmp));
}

// concat(s1: String, s2: String) -> String
static NativeFuncDefn(runtime_concat_SS) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &s1 = engine.arg(0);
  Cell &s2 = engine.arg(1);

  uint8_t *out = stringConcat(s1, s2, engine);
  engine.push(cellMakeHeapPtr(out));
}

// startsWith(s: String, prefix: String) -> Bool
static NativeFuncDefn(runtime_startsWith_SS) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);
  Cell &prefixCell = engine.arg(1);

  int64_t sLength = stringLength(sCell);
  uint8_t *sData = (uint8_t *)stringData(sCell);
  int64_t prefixLength = stringLength(prefixCell);
  uint8_t *prefixData = (uint8_t *)stringData(prefixCell);
  bool result = sLength >= prefixLength && !memcmp(sData, prefixData, prefixLength);

  engine.push(cellMakeBool(result));
}

// endsWith(s: String, suffix: String) -> Bool
static NativeFuncDefn(runtime_endsWith_SS) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);
  Cell &suffixCell = engine.arg(1);

  int64_t sLength = stringLength(sCell);
  uint8_t *sData = (uint8_t *)stringData(sCell);
  int64_t suffixLength = stringLength(suffixCell);
  uint8_t *suffixData = (uint8_t *)stringData(suffixCell);
  bool result = sLength >= suffixLength &&
                !memcmp(sData + (sLength - suffixLength), suffixData, suffixLength);

  engine.push(cellMakeBool(result));
}

// find(s: String, term: String, start: Int) -> Int
static NativeFuncDefn(runtime_find_SSI) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 3 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1)) ||
      !cellIsInt(engine.arg(2))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);
  Cell &termCell = engine.arg(1);
  Cell &startCell = engine.arg(2);

  int64_t sLength = stringLength(sCell);
  uint8_t *sData = (uint8_t *)stringData(sCell);
  int64_t termLength = stringLength(termCell);
  uint8_t *termData = (uint8_t *)stringData(termCell);
  int64_t start = cellInt(startCell);
  if (start < 0 || start > sLength) {
    BytecodeEngine::fatalError("Invalid argument");
  }

  void *p = memmem(sData + start, sLength - start, termData, termLength);
  int64_t pos = p ? (int64_t)((uint8_t *)p - sData) : -1;

  engine.push(cellMakeInt(pos));
}

// find(s: String, c: Int, start: Int) -> Int
static NativeFuncDefn(runtime_find_SII) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 3 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1)) ||
      !cellIsInt(engine.arg(2))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);
  Cell &termCell = engine.arg(1);
  Cell &startCell = engine.arg(2);

  int64_t sLength = stringLength(sCell);
  uint8_t *sData = (uint8_t *)stringData(sCell);
  int64_t term = cellInt(termCell);
  int64_t start = cellInt(startCell);
  if (start < 0 || start > sLength) {
    BytecodeEngine::fatalError("Invalid argument");
  }

  void *p = memchr(sData + start, (int)term & 0xff, sLength - start);
  int64_t pos = p ? (int64_t)((uint8_t *)p - sData) : -1;

  engine.push(cellMakeInt(pos));
}

// rfind(s: String, term: String, start: Int) -> Int
static NativeFuncDefn(runtime_rfind_SSI) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 3 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1)) ||
      !cellIsInt(engine.arg(2))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);
  Cell &termCell = engine.arg(1);
  Cell &startCell = engine.arg(2);

  int64_t sLength = stringLength(sCell);
  uint8_t *sData = (uint8_t *)stringData(sCell);
  int64_t termLength = stringLength(termCell);
  uint8_t *termData = (uint8_t *)stringData(termCell);
  int64_t start = cellInt(startCell);
  if (start < 0 || start > sLength - 1) {
    BytecodeEngine::fatalError("Invalid argument");
  }

  //~ this should use a more efficient string search algorithm
  int64_t pos = -1;
  for (int64_t i = std::min(start, sLength - termLength); i >= 0; --i) {
    if (!memcmp(sData + i, termData, termLength)) {
      pos = i;
      break;
    }
  }

  engine.push(cellMakeInt(pos));
}

// rfind(s: String, c: Int, start: Int) -> Int
static NativeFuncDefn(runtime_rfind_SII) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 3 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1)) ||
      !cellIsInt(engine.arg(2))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);
  Cell &termCell = engine.arg(1);
  Cell &startCell = engine.arg(2);

  int64_t sLength = stringLength(sCell);
  uint8_t *sData = (uint8_t *)stringData(sCell);
  uint8_t term = (uint8_t)cellInt(termCell);
  int64_t start = cellInt(startCell);
  if (start < 0 || start > sLength - 1) {
    BytecodeEngine::fatalError("Invalid argument");
  }

  // could use memrchr() here, but it's not available on some systems
  // (e.g., MacOS)
  int64_t pos = -1;
  for (int64_t i = start; i >= 0; --i) {
    if (sData[i] == term) {
      pos = i;
      break;
    }
  }

  engine.push(cellMakeInt(pos));
}

// substr(s: String, start: Int, n: Int) -> String
static NativeFuncDefn(runtime_substr_SII) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 3 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1)) ||
      !cellIsInt(engine.arg(2))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);
  Cell &startCell = engine.arg(1);
  Cell &nCell = engine.arg(2);

  int64_t sLength = stringLength(sCell);
  int64_t start = cellInt(startCell);
  int64_t n = cellInt(nCell);
  if (start < 0 || start > sLength ||
      n < 0 || start > sLength - n) {
    BytecodeEngine::fatalError("Invalid argument");
  }

  engine.push(stringMake(sCell, start, n, engine));
}

// toInt(s: String) -> Result[Int]
static NativeFuncDefn(runtime_toInt_S) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);

  int64_t val;
  if (stringToInt56Checked(stringToStdString(sCell), 10, val)) {
    engine.push(cellMakeInt(val));
  } else {
    engine.push(cellMakeError());
  }
}

// toInt(s: String, base: Int) -> Result[Int]
static NativeFuncDefn(runtime_toInt_SI) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);
  Cell &baseCell = engine.arg(1);

  int64_t base = cellInt(baseCell);
  if (base < 2 || base > 16) {
    BytecodeEngine::fatalError("Invalid argument");
  }

  int64_t val;
  if (stringToInt56Checked(stringToStdString(sCell), (int)base, val)) {
    engine.push(cellMakeInt(val));
  } else {
    engine.push(cellMakeError());
  }
}

// toFloat(s: String) -> Result[Float]
static NativeFuncDefn(runtime_toFloat_S) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);

  float val;
  if (stringToFloatChecked(stringToStdString(sCell), val)) {
    engine.push(cellMakeFloat(val));
  } else {
    engine.push(cellMakeError());
  }
}

// ifirst(s: String) -> Int
static NativeFuncDefn(runtime_ifirst_S) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &s = engine.arg(0);

  engine.push(cellMakeInt(0));
}

// imore(s: String, iter: Int) -> Bool
static NativeFuncDefn(runtime_imore_SI) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &s = engine.arg(0);
  Cell &iterCell = engine.arg(1);

  engine.push(cellMakeBool(cellInt(iterCell) < stringLength(s)));
}

//------------------------------------------------------------------------

void runtime_String_init(BytecodeEngine &engine) {
  engine.addNativeFunction("length_S", &runtime_length_S);
  engine.addNativeFunction("get_SI", &runtime_get_SI);
  engine.addNativeFunction("next_SI", &runtime_next_SI);
  engine.addNativeFunction("byte_SI", &runtime_byte_SI);
  engine.addNativeFunction("compare_SS", &runtime_compare_SS);
  engine.addNativeFunction("concat_SS", &runtime_concat_SS);
  engine.addNativeFunction("startsWith_SS", &runtime_startsWith_SS);
  engine.addNativeFunction("endsWith_SS", &runtime_endsWith_SS);
  engine.addNativeFunction("find_SSI", &runtime_find_SSI);
  engine.addNativeFunction("find_SII", &runtime_find_SII);
  engine.addNativeFunction("rfind_SSI", &runtime_rfind_SSI);
  engine.addNativeFunction("rfind_SII", &runtime_rfind_SII);
  engine.addNativeFunction("substr_SII", &runtime_substr_SII);
  engine.addNativeFunction("toInt_S", &runtime_toInt_S);
  engine.addNativeFunction("toInt_SI", &runtime_toInt_SI);
  engine.addNativeFunction("toFloat_S", &runtime_toFloat_S);
  engine.addNativeFunction("ifirst_S", &runtime_ifirst_S);
  engine.addNativeFunction("imore_SI", &runtime_imore_SI);
  engine.addNativeFunction("inext_SI", &runtime_next_SI);
  engine.addNativeFunction("iget_SI", &runtime_get_SI);
}

//------------------------------------------------------------------------
// support functions
//------------------------------------------------------------------------

int64_t stringLength(Cell &s) {
  void *sPtr = cellPtr(s);
  BytecodeEngine::failOnNilPtr(sPtr);
  return heapObjSize(sPtr);
}

uint8_t *stringData(Cell &s) {
  void *sPtr = cellPtr(s);
  BytecodeEngine::failOnNilPtr(sPtr);
  return (uint8_t *)sPtr + 8;
}

std::string stringToStdString(Cell &s) {
  void *sPtr = cellPtr(s);
  BytecodeEngine::failOnNilPtr(sPtr);
  return std::string((char *)sPtr + 8, heapObjSize(sPtr));
}

Cell stringAlloc(int64_t length, BytecodeEngine &engine) {
  return cellMakeHeapPtr(engine.heapAllocBlob((uint64_t)length, 0));
}

Cell stringMake(const uint8_t *data, int64_t length, BytecodeEngine &engine) {
  uint8_t *out = (uint8_t *)engine.heapAllocBlob((uint64_t)length, 0);
  memcpy(out + 8, data, length);
  return cellMakeHeapPtr(out);
}

Cell stringMake(Cell &s, int64_t offset, int64_t length, BytecodeEngine &engine) {
  uint8_t *out = (uint8_t *)engine.heapAllocBlob((uint64_t)length, 0);
  memcpy(out + 8, stringData(s) + offset, length);
  return cellMakeHeapPtr(out);
}

int64_t stringCompare(Cell &s1, Cell &s2) {
  int64_t n1 = stringLength(s1);
  int64_t n2 = stringLength(s2);
  uint8_t *p1 = (uint8_t *)stringData(s1);
  uint8_t *p2 = (uint8_t *)stringData(s2);
  int64_t n = std::min(n1, n2);
  for (int64_t i = 0; i < n; ++i) {
    if (p1[i] < p2[i]) {
      return -1;
    }
    if (p1[i] > p2[i]) {
      return 1;
      break;
    }
  }
  return (n1 < n2) ? -1 : (n1 > n2) ? 1 : 0;
}

uint8_t *stringConcat(Cell &s1, Cell &s2, BytecodeEngine &engine) {
  int64_t n1 = stringLength(s1);
  int64_t n2 = stringLength(s2);
  if (n1 > bytecodeMaxInt - n2) {
    BytecodeEngine::fatalError("Integer overflow");
  }
  uint8_t *out = (uint8_t *)engine.heapAllocBlob((uint64_t)(n1 + n2), 0);
  memcpy(out + 8, stringData(s1), n1);
  memcpy(out + 8 + n1, stringData(s2), n2);
  return out;
}
