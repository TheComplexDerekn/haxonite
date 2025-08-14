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
#include "runtime_Vector.h"

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

  int64_t sLength = stringByteLength(sCell);
  uint8_t *sData = (uint8_t *)stringData(sCell);
  int64_t prefixLength = stringByteLength(prefixCell);
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

  int64_t sLength = stringByteLength(sCell);
  uint8_t *sData = (uint8_t *)stringData(sCell);
  int64_t suffixLength = stringByteLength(suffixCell);
  uint8_t *suffixData = (uint8_t *)stringData(suffixCell);
  bool result = sLength >= suffixLength &&
                !memcmp(sData + (sLength - suffixLength), suffixData, suffixLength);

  engine.push(cellMakeBool(result));
}

// split(term: String, s: String) -> Vector[String]
static NativeFuncDefn(runtime_split_SS) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);
  Cell &termCell = engine.arg(1);

  Cell vCell = vectorMake(engine);
  engine.pushGCRoot(vCell);

  int64_t sLength = stringByteLength(sCell);
  int64_t termLength = stringByteLength(termCell);
  int64_t i = 0;
  while (true) {
    uint8_t *sData = stringData(sCell);
    uint8_t *p = (uint8_t *)memmem(sData + i, sLength - i, stringData(termCell), termLength);
    if (p) {
      int64_t j = (int64_t)(p - sData);
      Cell substrCell = stringMake(sCell, i, j - i, engine);
      vectorAppend(vCell, substrCell, engine);
      i = j + termLength;
    } else {
      Cell substrCell = stringMake(sCell, i, sLength - i, engine);
      vectorAppend(vCell, substrCell, engine);
      break;
    }
  }

  engine.push(vCell);
  engine.popGCRoot(vCell);
}

// splitFirst(term: String, s: String) -> Vector[String]
static NativeFuncDefn(runtime_splitFirst_SS) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);
  Cell &termCell = engine.arg(1);

  Cell vCell = vectorMake(engine);
  engine.pushGCRoot(vCell);

  uint8_t *sData = stringData(sCell);
  int64_t sLength = stringByteLength(sCell);
  int64_t termLength = stringByteLength(termCell);
  uint8_t *p = (uint8_t *)memmem(sData, sLength, stringData(termCell), termLength);
  if (p) {
    int64_t i = (int64_t)(p - sData);
    int64_t j = i + termLength;
    Cell substr1Cell = stringMake(sCell, 0, i, engine);
    vectorAppend(vCell, substr1Cell, engine);
    Cell substr2Cell = stringMake(sCell, j, sLength - j, engine);
    vectorAppend(vCell, substr2Cell, engine);
  } else {
    vectorAppend(vCell, sCell, engine);
  }

  engine.push(vCell);
  engine.popGCRoot(vCell);
}

// splitLast(term: String, s: String) -> Vector[String]
static NativeFuncDefn(runtime_splitLast_SS) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);
  Cell &termCell = engine.arg(1);

  Cell vCell = vectorMake(engine);
  engine.pushGCRoot(vCell);

  uint8_t *sData = stringData(sCell);
  int64_t sLength = stringByteLength(sCell);
  uint8_t *termData = stringData(termCell);
  int64_t termLength = stringByteLength(termCell);
  //~ this should use a more efficient string search algorithm
  int64_t i;
  for (i = sLength - termLength; i >= 0; --i) {
    if (!memcmp(sData + i, termData, termLength)) {
      break;
    }
  }
  if (i >= 0) {
    int64_t j = i + termLength;
    Cell substr1Cell = stringMake(sCell, 0, i, engine);
    vectorAppend(vCell, substr1Cell, engine);
    Cell substr2Cell = stringMake(sCell, j, sLength - j, engine);
    vectorAppend(vCell, substr2Cell, engine);
  } else {
    vectorAppend(vCell, sCell, engine);
  }

  engine.push(vCell);
  engine.popGCRoot(vCell);
}

// removePrefix(s: String, prefix: String) -> String
static NativeFuncDefn(runtime_removePrefix_SS) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);
  Cell &prefixCell = engine.arg(1);

  int64_t sLength = stringByteLength(sCell);
  uint8_t *sData = (uint8_t *)stringData(sCell);
  int64_t prefixLength = stringByteLength(prefixCell);
  uint8_t *prefixData = (uint8_t *)stringData(prefixCell);
  if (sLength >= prefixLength && !memcmp(sData, prefixData, prefixLength)) {
    engine.push(stringMake(sCell, prefixLength, sLength - prefixLength, engine));
  } else {
    engine.push(sCell);
  }
}

// removeSuffix(s: String, suffix: String) -> String
static NativeFuncDefn(runtime_removeSuffix_SS) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);
  Cell &suffixCell = engine.arg(1);

  int64_t sLength = stringByteLength(sCell);
  uint8_t *sData = (uint8_t *)stringData(sCell);
  int64_t suffixLength = stringByteLength(suffixCell);
  uint8_t *suffixData = (uint8_t *)stringData(suffixCell);
  if (sLength >= suffixLength &&
      !memcmp(sData + (sLength - suffixLength), suffixData, suffixLength)) {
    engine.push(stringMake(sCell, 0, sLength - suffixLength, engine));
  } else {
    engine.push(sCell);
  }
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

// byteLength(s: String) -> Int
static NativeFuncDefn(runtime_byteLength_S) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &s = engine.arg(0);

  engine.push(cellMakeInt(stringByteLength(s)));
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
  int64_t length = stringByteLength(s);
  if (idx < 0 || idx >= length) {
    BytecodeEngine::fatalError("Index out of bounds");
  }
  uint8_t *sData = (uint8_t *)stringData(s);

  engine.push(cellMakeInt(sData[idx]));
}

// codepoint(s: String, idx: Int) -> Int
static NativeFuncDefn(runtime_codepoint_SI) {
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
  int64_t length = stringByteLength(s);
  if (idx < 0 || idx >= length) {
    BytecodeEngine::fatalError("Index out of bounds");
  }
  uint32_t u;
  if (!utf8Get(stringData(s), length, idx, u)) {
    BytecodeEngine::fatalError("Index out of bounds");
  }
  engine.push(cellMakeInt((int64_t)u));
}

// nextCodepoint(s: String, idx: Int) -> Int
static NativeFuncDefn(runtime_nextCodepoint_SI) {
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
  int64_t length = stringByteLength(s);
  if (idx < 0 || idx >= length) {
    BytecodeEngine::fatalError("Index out of bounds");
  }
  int n = utf8Length(stringData(s), length, idx);
  engine.push(cellMakeInt(idx + n));
}

// substr(s: String, first: Int, last: Int) -> String
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
  Cell &firstCell = engine.arg(1);
  Cell &lastCell = engine.arg(2);

  int64_t sLength = stringByteLength(sCell);
  int64_t first = cellInt(firstCell);
  int64_t last = cellInt(lastCell);
  if (first < 0 || first > sLength ||
      last < first || last > sLength) {
    BytecodeEngine::fatalError("Invalid argument");
  }

  engine.push(stringMake(sCell, first, last - first, engine));
}

// codepointToString(c: Int) -> String
static NativeFuncDefn(runtime_codepointToString_I) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsInt(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &cCell = engine.arg(0);

  int64_t c = cellInt(cCell);
  uint8_t u[utf8MaxBytes];
  int uLen = utf8Encode((uint32_t)c, u);

  engine.push(stringMake(u, uLen, engine));
}

//------------------------------------------------------------------------

void runtime_String_init(BytecodeEngine &engine) {
  engine.addNativeFunction("compare_SS", &runtime_compare_SS);
  engine.addNativeFunction("concat_SS", &runtime_concat_SS);
  engine.addNativeFunction("startsWith_SS", &runtime_startsWith_SS);
  engine.addNativeFunction("endsWith_SS", &runtime_endsWith_SS);
  engine.addNativeFunction("split_SS", &runtime_split_SS);
  engine.addNativeFunction("splitFirst_SS", &runtime_splitFirst_SS);
  engine.addNativeFunction("splitLast_SS", &runtime_splitLast_SS);
  engine.addNativeFunction("removePrefix_SS", &runtime_removePrefix_SS);
  engine.addNativeFunction("removeSuffix_SS", &runtime_removeSuffix_SS);
  engine.addNativeFunction("toInt_S", &runtime_toInt_S);
  engine.addNativeFunction("toInt_SI", &runtime_toInt_SI);
  engine.addNativeFunction("toFloat_S", &runtime_toFloat_S);
  engine.addNativeFunction("byteLength_S", &runtime_byteLength_S);
  engine.addNativeFunction("byte_SI", &runtime_byte_SI);
  engine.addNativeFunction("codepoint_SI", &runtime_codepoint_SI);
  engine.addNativeFunction("nextCodepoint_SI", &runtime_nextCodepoint_SI);
  engine.addNativeFunction("substr_SII", &runtime_substr_SII);
  engine.addNativeFunction("codepointToString_I", &runtime_codepointToString_I);
}

//------------------------------------------------------------------------
// support functions
//------------------------------------------------------------------------

int64_t stringByteLength(Cell &s) {
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
  int64_t n1 = stringByteLength(s1);
  int64_t n2 = stringByteLength(s2);
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
  int64_t n1 = stringByteLength(s1);
  int64_t n2 = stringByteLength(s2);
  if (n1 > bytecodeMaxInt - n2) {
    BytecodeEngine::fatalError("Integer overflow");
  }
  uint8_t *out = (uint8_t *)engine.heapAllocBlob((uint64_t)(n1 + n2), 0);
  memcpy(out + 8, stringData(s1), n1);
  memcpy(out + 8 + n1, stringData(s2), n2);
  return out;
}
