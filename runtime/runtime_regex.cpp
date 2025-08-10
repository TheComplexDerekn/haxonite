//========================================================================
//
// runtime_regex.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "runtime_regex.h"
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include "runtime_String.h"
#include "runtime_Vector.h"

//------------------------------------------------------------------------

static pcre2_code *makeRE(Cell &reCell, BytecodeEngine &engine) {
  uint8_t *reData = stringData(reCell);
  int64_t reLength = stringByteLength(reCell);
  if (reLength > PCRE2_SIZE_MAX) {
    BytecodeEngine::fatalError("Integer overflow");
  }
  int errorCode;
  PCRE2_SIZE errorOffset;
  return pcre2_compile((PCRE2_SPTR)reData, (PCRE2_SIZE)reLength,
		       PCRE2_UTF, &errorCode, &errorOffset, nullptr);
}

static void freeRE(pcre2_code *re) {
  pcre2_code_free(re);
}

// reTest(re: String, s: String) -> Result[Bool]
static NativeFuncDefn(runtime_reTest_SS) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &reCell = engine.arg(0);
  Cell &sCell = engine.arg(1);

  pcre2_code *re = makeRE(reCell, engine);
  if (re) {
    uint8_t *sData = stringData(sCell);
    int64_t sLength = stringByteLength(sCell);
    if (sLength > PCRE2_SIZE_MAX) {
      BytecodeEngine::fatalError("Integer overflow");
    }
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, nullptr);
    if (!md) {
      BytecodeEngine::fatalError("Out of memory");
    }
    int n = pcre2_match(re, (PCRE2_SPTR)sData, (PCRE2_SIZE)sLength, 0, 0, md, nullptr);
    pcre2_match_data_free(md);
    freeRE(re);
    engine.push(cellMakeBool(n > 0));
  } else {
    engine.push(cellMakeError());
  }
}

// reMatch(re: String, s: String) -> Result[Vector[String]]
static NativeFuncDefn(runtime_reMatch_SS) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &reCell = engine.arg(0);
  Cell &sCell = engine.arg(1);

  pcre2_code *re = makeRE(reCell, engine);
  if (re) {
    uint8_t *sData = stringData(sCell);
    int64_t sLength = stringByteLength(sCell);
    if (sLength > PCRE2_SIZE_MAX) {
      BytecodeEngine::fatalError("Integer overflow");
    }
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, nullptr);
    if (!md) {
      BytecodeEngine::fatalError("Out of memory");
    }
    PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);
    int n = pcre2_match(re, (PCRE2_SPTR)sData, (PCRE2_SIZE)sLength, 0, 0, md, nullptr);
    Cell vCell = vectorMake(engine);
    engine.pushGCRoot(vCell);
    for (int i = 0; i < n; ++i) {
      Cell mCell;
      if (ov[2*i] == PCRE2_UNSET || ov[2*i + 1] == PCRE2_UNSET) {
	mCell = stringMake((const uint8_t *)"", 0, engine);
      } else {
	mCell = stringMake(sCell, ov[2*i], ov[2*i + 1] - ov[2*i], engine);
      }
      vectorAppend(vCell, mCell, engine);
    }
    pcre2_match_data_free(md);
    freeRE(re);
    engine.push(vCell);
    engine.popGCRoot(vCell);
  } else {
    engine.push(cellMakeError());
  }
}

// reSplit(re: String, s: String) -> Result[Vector[String]]
static NativeFuncDefn(runtime_reSplit_SS) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &reCell = engine.arg(0);
  Cell &sCell = engine.arg(1);

  int64_t sLength = stringByteLength(sCell);
  if (sLength > PCRE2_SIZE_MAX) {
    BytecodeEngine::fatalError("Integer overflow");
  }

  pcre2_code *re = makeRE(reCell, engine);
  if (re) {
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, nullptr);
    if (!md) {
      BytecodeEngine::fatalError("Out of memory");
    }
    PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);

    Cell vCell = vectorMake(engine);
    engine.pushGCRoot(vCell);

    int64_t pos = 0;
    while (true) {
      uint8_t *sData = stringData(sCell);
      int n = pcre2_match(re, (PCRE2_SPTR)sData, (PCRE2_SIZE)sLength, (PCRE2_SIZE)pos,
			  0, md, nullptr);
      if (n <= 0 || (int64_t)ov[1] <= pos) {
	Cell mCell = stringMake(sData + pos, sLength - pos, engine);
	vectorAppend(vCell, mCell, engine);
	break;
      }
      Cell mCell = stringMake(sCell, pos, (int64_t)ov[0] - pos, engine);
      vectorAppend(vCell, mCell, engine);
      pos = (int64_t)ov[1];
    }

    pcre2_match_data_free(md);
    freeRE(re);
    engine.push(vCell);
    engine.popGCRoot(vCell);

  } else {
    engine.push(cellMakeError());
  }
}

// reReplace(re: String, s: String, sub: String) -> Result[String]
static NativeFuncDefn(runtime_reReplace_SSS) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 3 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1)) ||
      !cellIsPtr(engine.arg(2))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &reCell = engine.arg(0);
  Cell &sCell = engine.arg(1);
  Cell &subCell = engine.arg(2);

  uint8_t *sData = stringData(sCell);
  int64_t sLength = stringByteLength(sCell);
  if (sLength > PCRE2_SIZE_MAX) {
    BytecodeEngine::fatalError("Integer overflow");
  }

  uint8_t *subData = stringData(subCell);
  int64_t subLength = stringByteLength(subCell);

  pcre2_code *re = makeRE(reCell, engine);
  if (re) {
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, nullptr);
    if (!md) {
      BytecodeEngine::fatalError("Out of memory");
    }
    PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);

    std::string out;

    int64_t pos = 0;
    while (true) {
      int n = pcre2_match(re, (PCRE2_SPTR)sData, (PCRE2_SIZE)sLength, (PCRE2_SIZE)pos,
			  0, md, nullptr);
      if (n <= 0 || (int64_t)ov[1] <= pos) {
	out.append((char *)sData + pos, sLength - pos);
	break;
      }
      out.append((char *)sData + pos, (int64_t)ov[0] - pos);
      out.append((char *)subData, subLength);
      pos = (int64_t)ov[1];
    }

    pcre2_match_data_free(md);
    freeRE(re);
    engine.push(stringMake((uint8_t *)out.c_str(), (int64_t)out.size(), engine));

  } else {
    engine.push(cellMakeError());
  }
}

//------------------------------------------------------------------------

void runtime_regex_init(BytecodeEngine &engine) {
  engine.addNativeFunction("reTest_SS", &runtime_reTest_SS);
  engine.addNativeFunction("reMatch_SS", &runtime_reMatch_SS);
  engine.addNativeFunction("reSplit_SS", &runtime_reSplit_SS);
  engine.addNativeFunction("reReplace_SSS", &runtime_reReplace_SSS);
}
