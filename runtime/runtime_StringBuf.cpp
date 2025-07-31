//========================================================================
//
// runtime_StringBuf.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

// A StringBuf is implemented as a handle that points to a blob of
// bytes.
//
// +--------+---------+
// | length | pointer |
// +--------+---------+
// handle        |
//               v
//          +------+-------+-------+-----+---------+------//------+
//          | size | sb[0] | sb[1] | ... | sb[n-1] | unused space |
//          +------+-------+-------+-----+---------+--------------+
//          blob
//
// - The length field in the handle is the number of valid bytes in
//   the StringBuf.
// - The size field in the blob is the number of bytes in the blob,
//   i.e., the StringBuf size (capacity).
// - There are size-length unused bytes at the end of the blob.
//
// As a special case, an empty StringBuf is a handle with a nil
// pointer:
//
// +----------+-------------+
// | length=0 | pointer=nil |
// +----------+-------------+

#include "runtime_StringBuf.h"
#include <string.h>
#include "BytecodeDefs.h"
#include "runtime_String.h"
#include "UTF8.h"

//------------------------------------------------------------------------

#define minStringBufSize 16

struct StringBufHandle {
  uint64_t hdr;
  Cell dataPtr;
};

struct StringBufData {
  uint64_t hdr;
  uint8_t bytes[0];
};

//------------------------------------------------------------------------

// Expand the StringBuf in [vCell] to fit [newLength] elements. This
// may change the StringBuf's size (capacity), but will not change its
// length; the caller is responsible for changing the StringBuf length
// to [newLength]. This function may trigger GC.
static void stringBufExpand(Cell &sbCell, int64_t newLength, BytecodeEngine &engine) {
  StringBufHandle *sb = (StringBufHandle *)cellPtr(sbCell);
  engine.failOnNilPtr(sb);
  StringBufData *data = (StringBufData *)cellPtr(sb->dataPtr);
  int64_t length = heapObjSize(sb);
  int64_t size = data ? heapObjSize(data) : 0;
  if (newLength <= size) {
    return;
  }

  int64_t newSize = size ? size : minStringBufSize;
  while (newSize < newLength) {
    if (newSize > bytecodeMaxInt / 2) {
      BytecodeEngine::fatalError("Integer overflow");
    }
    newSize *= 2;
  }

  // NB: this may trigger GC
  StringBufData *newData = (StringBufData *)engine.heapAllocBlob(newSize, 0);

  sb = (StringBufHandle *)cellPtr(sbCell);
  if (length > 0) {
    data = (StringBufData *)cellPtr(sb->dataPtr);
    memcpy(newData->bytes, data->bytes, length);
  }
  sb->dataPtr = cellMakeHeapPtr(newData);
}

// Shrink the StringBuf in [sbCell] to fit its length. If the
// StringBuf's size (capacity) is sufficiently larger than its length,
// the size will be reduced. This will not change the StringBuf's
// length.  This function may trigger GC.
static void stringBufShrink(Cell &sbCell, BytecodeEngine &engine) {
  StringBufHandle *sb = (StringBufHandle *)cellPtr(sbCell);
  engine.failOnNilPtr(sb);
  StringBufData *data = (StringBufData *)cellPtr(sb->dataPtr);
  int64_t length = heapObjSize(sb);
  int64_t size = data ? heapObjSize(data) : 0;
  if (size <= minStringBufSize || size / 4 < length) {
    return;
  }

  int64_t newSize = size;
  do {
    newSize /= 2;
  } while (newSize / 4 >= length && newSize > minStringBufSize);

  // NB: this may trigger GC
  StringBufData *newData = (StringBufData *)engine.heapAllocBlob(newSize, 0);

  sb = (StringBufHandle *)cellPtr(sbCell);
  if (length > 0) {
    data = (StringBufData *)cellPtr(sb->dataPtr);
    memcpy(newData->bytes, data->bytes, length);
  }
  sb->dataPtr = cellMakeHeapPtr(newData);
}

static NativeFuncDefn(runtime_allocStringBuf) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 0) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  StringBufHandle *sb = (StringBufHandle *)engine.heapAllocHandle(0, 0);
  sb->dataPtr = cellMakeNilHeapPtr();
  engine.push(cellMakeHeapPtr(sb));
}

// length(sb: StringBuf) -> Int
static NativeFuncDefn(runtime_length_T) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sbCell = engine.arg(0);

  StringBufHandle *sb = (StringBufHandle *)cellPtr(sbCell);
  engine.failOnNilPtr(sb);
  int64_t length = heapObjSize(sb);

  engine.push(cellMakeInt(length));
}

// ulength(sb: StringBuf) -> Int
static NativeFuncDefn(runtime_ulength_T) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sbCell = engine.arg(0);

  StringBufHandle *sb = (StringBufHandle *)cellPtr(sbCell);
  engine.failOnNilPtr(sb);
  int64_t length = heapObjSize(sb);
  StringBufData *data = (StringBufData *)cellPtr(sb->dataPtr);

  int64_t i = 0;
  int64_t n = 0;
  while (i < length) {
    i += utf8Length(data->bytes, length, i);
    ++n;
  }
  engine.push(cellMakeInt(n));
}

// get(sb: StringBuf, idx: Int) -> Int
// iget(sb: StringBuf, iter: Int) -> Int
static NativeFuncDefn(runtime_get_TI) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sbCell = engine.arg(0);
  Cell &idxCell = engine.arg(1);

  StringBufHandle *sb = (StringBufHandle *)cellPtr(sbCell);
  engine.failOnNilPtr(sb);
  int64_t idx = cellInt(idxCell);

  int64_t length = heapObjSize(sb);
  if (idx < 0 || idx >= length) {
    BytecodeEngine::fatalError("Index out of bounds");
  }
  StringBufData *data = (StringBufData *)cellPtr(sb->dataPtr);
  uint32_t u;
  if (!utf8Get(data->bytes, length, idx, u)) {
    BytecodeEngine::fatalError("Index out of bounds");
  }
  engine.push(cellMakeInt((int64_t)u));
}

// byte(sb: StringBuf, idx: Int) -> Int
static NativeFuncDefn(runtime_byte_TI) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sbCell = engine.arg(0);
  Cell &idxCell = engine.arg(1);

  StringBufHandle *sb = (StringBufHandle *)cellPtr(sbCell);
  engine.failOnNilPtr(sb);
  int64_t idx = cellInt(idxCell);

  int64_t length = heapObjSize(sb);
  if (idx < 0 || idx >= length) {
    BytecodeEngine::fatalError("Index out of bounds");
  }
  StringBufData *data = (StringBufData *)cellPtr(sb->dataPtr);
  engine.push(cellMakeInt(data->bytes[idx]));
}

// next(sb: StringBuf, idx: Int) -> Int
// inext(sb: StringBuf, iter: Int) -> Int
static NativeFuncDefn(runtime_next_TI) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sbCell = engine.arg(0);
  Cell &idxCell = engine.arg(1);

  StringBufHandle *sb = (StringBufHandle *)cellPtr(sbCell);
  engine.failOnNilPtr(sb);
  int64_t idx = cellInt(idxCell);

  int64_t length = heapObjSize(sb);
  if (idx < 0 || idx >= length) {
    BytecodeEngine::fatalError("Index out of bounds");
  }
  StringBufData *data = (StringBufData *)cellPtr(sb->dataPtr);
  int n = utf8Length(data->bytes, length, idx);
  engine.push(cellMakeInt(idx + n));
}

// append(sb: StringBuf, c: Int)
static NativeFuncDefn(runtime_append_TI) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sbCell = engine.arg(0);
  Cell &cCell = engine.arg(1);

  int64_t c = cellInt(cCell);
  uint8_t u[utf8MaxBytes];
  int uLen = utf8Encode(c, u);
  if (uLen > 0) {
    // NB: this may trigger GC
    stringBufAppend(sbCell, u, uLen, engine);
  }

  engine.push(cellMakeInt(0));
}

// append(sb: StringBuf, s: String)
static NativeFuncDefn(runtime_append_TS) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sbCell = engine.arg(0);
  Cell &sCell = engine.arg(1);

  // NB: this may trigger GC
  stringBufAppendString(sbCell, sCell, engine);

  engine.push(cellMakeInt(0));
}

// append(sb: StringBuf, other: StringBuf)
static NativeFuncDefn(runtime_append_TT) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sbCell = engine.arg(0);
  Cell &otherCell = engine.arg(1);

  // can't use stringBufAppend here, because GC can invalidate the
  // string data pointer

  StringBufHandle *sb = (StringBufHandle *)cellPtr(sbCell);
  engine.failOnNilPtr(sb);
  int64_t sbLength = heapObjSize(sb);
  StringBufHandle *other = (StringBufHandle *)cellPtr(otherCell);
  engine.failOnNilPtr(sb);
  int64_t otherLength = heapObjSize(other);
  if (sbLength > bytecodeMaxInt - otherLength) {
    BytecodeEngine::fatalError("Invalid argument");
  }

  // NB: this may trigger GC
  stringBufExpand(sbCell, sbLength + otherLength, engine);

  sb = (StringBufHandle *)cellPtr(sbCell);
  StringBufData *sbData = (StringBufData *)cellPtr(sb->dataPtr);
  other = (StringBufHandle *)cellPtr(otherCell);
  StringBufData *otherData = (StringBufData *)cellPtr(other->dataPtr);
  memcpy(sbData->bytes + sbLength, otherData->bytes, otherLength);
  heapObjSetSize(sb, sbLength + otherLength);
  engine.push(cellMakeInt(0));
}

// clear(sb: StringBuf)
static NativeFuncDefn(runtime_clear_T) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sbCell = engine.arg(0);

  StringBufHandle *sb = (StringBufHandle *)cellPtr(sbCell);
  engine.failOnNilPtr(sb);
  heapObjSetSize(sb, 0);

  // NB: this may trigger GC
  stringBufShrink(sbCell, engine);

  engine.push(cellMakeInt(0));
}

// toString(sb: StringBuf) -> String
static NativeFuncDefn(runtime_toString_T) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sbCell = engine.arg(0);

  StringBufHandle *sb = (StringBufHandle *)cellPtr(sbCell);
  engine.failOnNilPtr(sb);
  int64_t sbLength = heapObjSize(sb);

  // NB: this may trigger GC
  Cell s = stringAlloc(sbLength, engine);

  sb = (StringBufHandle *)cellPtr(sbCell);
  StringBufData *data = (StringBufData *)cellPtr(sb->dataPtr);
  memcpy(stringData(s), data->bytes, sbLength);
  engine.push(s);
}

// ifirst(sb: StringBuf) -> Int
static NativeFuncDefn(runtime_ifirst_T) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sbCell = engine.arg(0);

  engine.push(cellMakeInt(0));
}

// imore(sb: StringBuf, iter: Int) -> Bool
static NativeFuncDefn(runtime_imore_TI) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sbCell = engine.arg(0);
  Cell &iterCell = engine.arg(1);

  StringBufHandle *sb = (StringBufHandle *)cellPtr(sbCell);
  engine.failOnNilPtr(sb);
  int64_t iter = cellInt(iterCell);
  int64_t length = heapObjSize(sb);
  engine.push(cellMakeBool(iter < length));
}

//------------------------------------------------------------------------

void runtime_StringBuf_init(BytecodeEngine &engine) {
  engine.addNativeFunction("_allocStringBuf", &runtime_allocStringBuf);
  engine.addNativeFunction("length_T", &runtime_length_T);
  engine.addNativeFunction("ulength_T", &runtime_ulength_T);
  engine.addNativeFunction("get_TI", &runtime_get_TI);
  engine.addNativeFunction("byte_TI", &runtime_byte_TI);
  engine.addNativeFunction("next_TI", &runtime_next_TI);
  engine.addNativeFunction("append_TI", &runtime_append_TI);
  engine.addNativeFunction("append_TS", &runtime_append_TS);
  engine.addNativeFunction("append_TT", &runtime_append_TT);
  engine.addNativeFunction("clear_T", &runtime_clear_T);
  engine.addNativeFunction("toString_T", &runtime_toString_T);
  engine.addNativeFunction("ifirst_T", &runtime_ifirst_T);
  engine.addNativeFunction("imore_TI", &runtime_imore_TI);
  engine.addNativeFunction("inext_TI", &runtime_next_TI);
  engine.addNativeFunction("iget_TI", &runtime_get_TI);
}

//------------------------------------------------------------------------
// support functions
//------------------------------------------------------------------------

int64_t stringBufLength(Cell &sbCell) {
  StringBufHandle *sb = (StringBufHandle *)cellPtr(sbCell);
  BytecodeEngine::failOnNilPtr(sb);
  return heapObjSize(sb);
}

uint8_t *stringBufData(Cell &sbCell) {
  StringBufHandle *sb = (StringBufHandle *)cellPtr(sbCell);
  BytecodeEngine::failOnNilPtr(sb);
  StringBufData *sbData = (StringBufData *)cellPtr(sb->dataPtr);
  return sbData->bytes;
}

void stringBufAppend(Cell &sbCell, uint8_t *buf, int64_t n, BytecodeEngine &engine) {
  StringBufHandle *sb = (StringBufHandle *)cellPtr(sbCell);
  engine.failOnNilPtr(sb);
  int64_t length = heapObjSize(sb);
  if (n > bytecodeMaxInt - length) {
    BytecodeEngine::fatalError("Integer overflow");
  }

  // NB: this may trigger GC
  stringBufExpand(sbCell, length + n, engine);

  sb = (StringBufHandle *)cellPtr(sbCell);
  StringBufData *data = (StringBufData *)cellPtr(sb->dataPtr);
  memcpy(data->bytes + length, buf, n);
  heapObjSetSize(sb, length + n);
}

void stringBufAppendString(Cell &sbCell, Cell &sCell, BytecodeEngine &engine) {
  StringBufHandle *sb = (StringBufHandle *)cellPtr(sbCell);
  engine.failOnNilPtr(sb);
  int64_t sbLength = heapObjSize(sb);
  int64_t sLength = stringLength(sCell);
  if (sbLength > bytecodeMaxInt - sLength) {
    BytecodeEngine::fatalError("Integer overflow");
  }

  // NB: this may trigger GC
  stringBufExpand(sbCell, sbLength + sLength, engine);

  sb = (StringBufHandle *)cellPtr(sbCell);
  StringBufData *data = (StringBufData *)cellPtr(sb->dataPtr);
  memcpy(data->bytes + sbLength, stringData(sCell), sLength);
  heapObjSetSize(sb, sbLength + sLength);
}
