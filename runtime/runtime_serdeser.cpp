//========================================================================
//
// runtime_serdeser.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "runtime_serdeser.h"
#include <string.h>
#include "BytecodeDefs.h"
#include "runtime_String.h"
#include "runtime_StringBuf.h"

//------------------------------------------------------------------------

struct DeserBuf {
  uint64_t hdr;
  Cell data;
  Cell pos;
};

//------------------------------------------------------------------------

// ser(val: Int, out: StringBuf)
static NativeFuncDefn(runtime_ser_IT) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsInt(engine.arg(0)) ||
      !cellIsHeapPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &valCell = engine.arg(0);
  Cell &outCell = engine.arg(1);

  int64_t val = cellInt(valCell);
  stringBufAppend(outCell, (uint8_t *)&val, 8, engine);

  engine.push(cellMakeInt(0));
}

// ser(val: Float, out: StringBuf)
static NativeFuncDefn(runtime_ser_FT) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsFloat(engine.arg(0)) ||
      !cellIsHeapPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &valCell = engine.arg(0);
  Cell &outCell = engine.arg(1);

  float val = cellFloat(valCell);
  stringBufAppend(outCell, (uint8_t *)&val, 4, engine);

  engine.push(cellMakeInt(0));
}

// ser(val: Bool, out: StringBuf)
static NativeFuncDefn(runtime_ser_BT) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsBool(engine.arg(0)) ||
      !cellIsHeapPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &valCell = engine.arg(0);
  Cell &outCell = engine.arg(1);

  uint8_t val = cellBool(valCell) ? 1 : 0;
  stringBufAppend(outCell, &val, 1, engine);

  engine.push(cellMakeInt(0));
}

// ser(val: String, out: StringBuf)
static NativeFuncDefn(runtime_ser_ST) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsHeapPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &valCell = engine.arg(0);
  Cell &outCell = engine.arg(1);

  int64_t length = stringByteLength(valCell);
  stringBufAppend(outCell, (uint8_t *)&length, 8, engine);
  stringBufAppendString(outCell, valCell, engine);

  engine.push(cellMakeInt(0));
}

// serHeader(hdr: String, out: StringBuf)
static NativeFuncDefn(runtime_serHeader_ST) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsHeapPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &hdrCell = engine.arg(0);
  Cell &outCell = engine.arg(1);

  stringBufAppendString(outCell, hdrCell, engine);

  engine.push(cellMakeInt(0));
}

// deserInt(in: DeserBuf) -> Result[Int]
static NativeFuncDefn(runtime_deserInt_8DeserBuf) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &inCell = engine.arg(0);

  DeserBuf *in = (DeserBuf *)cellHeapPtr(inCell);
  engine.failOnNilPtr(in);
  int64_t pos = cellInt(in->pos);
  if (pos <= stringBufLength(in->data) - 8) {
    uint8_t bytes[8];
    memcpy(bytes, stringBufData(in->data) + pos, 8);
    int64_t val = *(int64_t *)bytes;
    if (val >= bytecodeMinInt && val <= bytecodeMaxInt) {
      in->pos = cellMakeInt(pos + 8);
      engine.push(cellMakeInt(val));
    } else {
      engine.push(cellMakeError());
    }
  } else {
    engine.push(cellMakeError());
  }
}

// deserFloat(in: DeserBuf) -> Result[Float]
static NativeFuncDefn(runtime_deserFloat_8DeserBuf) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &inCell = engine.arg(0);

  DeserBuf *in = (DeserBuf *)cellHeapPtr(inCell);
  engine.failOnNilPtr(in);
  int64_t pos = cellInt(in->pos);
  if (pos <= stringBufLength(in->data) - 4) {
    uint8_t bytes[4];
    memcpy(bytes, stringBufData(in->data) + pos, 4);
    float val = *(float *)bytes;
    engine.push(cellMakeFloat(val));
    in->pos = cellMakeInt(pos + 4);
  } else {
    engine.push(cellMakeError());
  }
}

// deserBool(in: DeserBuf) -> Result[Bool]
static NativeFuncDefn(runtime_deserBool_8DeserBuf) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &inCell = engine.arg(0);

  DeserBuf *in = (DeserBuf *)cellHeapPtr(inCell);
  engine.failOnNilPtr(in);
  int64_t pos = cellInt(in->pos);
  if (pos <= stringBufLength(in->data) - 1) {
    uint8_t byte = stringBufData(in->data)[pos];
    if (byte == 0) {
      engine.push(cellMakeBool(false));
      in->pos = cellMakeInt(pos + 1);
    } else if (byte == 1) {
      engine.push(cellMakeBool(true));
      in->pos = cellMakeInt(pos + 1);
    } else {
      engine.push(cellMakeError());
    }
  } else {
    engine.push(cellMakeError());
  }
}

// deserString(in: DeserBuf) -> Result[String]
static NativeFuncDefn(runtime_deserString_8DeserBuf) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &inCell = engine.arg(0);

  DeserBuf *in = (DeserBuf *)cellHeapPtr(inCell);
  engine.failOnNilPtr(in);
  int64_t pos = cellInt(in->pos);
  int64_t length = stringBufLength(in->data);

  if (pos <= length - 8) {
    uint8_t bytes[8];
    memcpy(bytes, stringBufData(in->data) + pos, 8);
    int64_t n = *(int64_t *)bytes;
    if (n >= bytecodeMinInt && n <= bytecodeMaxInt) {
      pos += 8;
      if (pos <= length - n) {
	// NB: this may trigger GC
	Cell s = stringAlloc(n, engine);
	in = (DeserBuf *)cellHeapPtr(inCell);
	memcpy(stringData(s), stringBufData(in->data) + pos, n);
	engine.push(s);
	in->pos = cellMakeInt(pos + n);
      } else {
	engine.push(cellMakeError());
      }
    } else {
      engine.push(cellMakeError());
    }
  } else {
    engine.push(cellMakeError());
  }
}

// deserHeader(hdr: String, in: DeserBuf) -> Result[]
static NativeFuncDefn(runtime_deserHeader_S8DeserBuf) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsHeapPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &hdrCell = engine.arg(0);
  Cell &inCell = engine.arg(1);

  DeserBuf *in = (DeserBuf *)cellHeapPtr(inCell);
  engine.failOnNilPtr(in);
  int64_t pos = cellInt(in->pos);
  int64_t length = stringBufLength(in->data);
  int64_t n = stringByteLength(hdrCell);

  if (pos <= length - n) {
    if (memcmp(stringBufData(in->data) + pos, stringData(hdrCell), n) == 0) {
      engine.push(cellMakeInt(0));
      in->pos = cellMakeInt(pos + n);
    } else {
      engine.push(cellMakeError());
    }
  } else {
    engine.push(cellMakeError());
  }
}

// deserEnd(in: DeserBuf) -> Result[]
static NativeFuncDefn(runtime_deserEnd_8DeserBuf) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &inCell = engine.arg(0);

  DeserBuf *in = (DeserBuf *)cellHeapPtr(inCell);
  engine.failOnNilPtr(in);
  int64_t pos = cellInt(in->pos);
  if (pos == stringBufLength(in->data)) {
    engine.push(cellMakeInt(0));
  } else {
    engine.push(cellMakeError());
  }
}

//------------------------------------------------------------------------

void runtime_serdeser_init(BytecodeEngine &engine) {
  engine.addNativeFunction("ser_IT", &runtime_ser_IT);
  engine.addNativeFunction("ser_FT", &runtime_ser_FT);
  engine.addNativeFunction("ser_BT", &runtime_ser_BT);
  engine.addNativeFunction("ser_ST", &runtime_ser_ST);
  engine.addNativeFunction("serHeader_ST", &runtime_serHeader_ST);
  engine.addNativeFunction("deserInt_8DeserBuf", &runtime_deserInt_8DeserBuf);
  engine.addNativeFunction("deserFloat_8DeserBuf", &runtime_deserFloat_8DeserBuf);
  engine.addNativeFunction("deserBool_8DeserBuf", &runtime_deserBool_8DeserBuf);
  engine.addNativeFunction("deserString_8DeserBuf", &runtime_deserString_8DeserBuf);
  engine.addNativeFunction("deserHeader_S8DeserBuf", &runtime_deserHeader_S8DeserBuf);
  engine.addNativeFunction("deserEnd_8DeserBuf", &runtime_deserEnd_8DeserBuf);
}
