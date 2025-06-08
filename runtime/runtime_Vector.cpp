//========================================================================
//
// runtime_Vector.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

// A vector is implemented as a handle that points to a tuple of
// cells:
//
// +--------+---------+
// | length | pointer |
// +--------+---------+
// handle        |
//               v
//          +--------+------+------+-----+--------+------//------+
//          | size*8 | v[0] | v[1] | ... | v[n-1] | unused space |
//          +--------+------+------+-----+--------+--------------+
//          tuple
//
// - The length field in the handle is the number of valid elements in
//   the vector.
// - The size field in the tuple is the number of bytes in the tuple,
//   i.e., 8 * the vector size (capacity).
// - There are size-length unused cells at the end of the tuple.
//
// As a special case, an empty vector is a handle with a nil pointer:
//
// +----------+-------------+
// | length=0 | pointer=nil |
// +----------+-------------+

#include "runtime_Vector.h"
#include <string.h>
#include <algorithm>
#include "BytecodeDefs.h"

//------------------------------------------------------------------------

#define minVectorSize 8

struct VectorHandle {
  uint64_t hdr;
  Cell dataPtr;
};

struct VectorData {
  uint64_t hdr;
  Cell elems[0];
};
#define bytesPerElement 8

//------------------------------------------------------------------------

class VectorIter {
public:

  using difference_type = int64_t;
  using value_type = Cell;
  using pointer = Cell*;
  using reference = Cell&;
  using iterator_category = std::random_access_iterator_tag;

  VectorIter(Cell &aVCell, int64_t aIdx)
    : vCell(aVCell), idx(aIdx) {}
  VectorIter(const VectorIter &other)
    : vCell(other.vCell), idx(other.idx) {}
  VectorIter &operator=(const VectorIter &other) {
    vCell = other.vCell;
    idx = other.idx;
    return *this;
  }
  Cell &operator*() {
    VectorHandle *v = (VectorHandle *)cellPtr(vCell);
    VectorData *data = (VectorData *)cellPtr(v->dataPtr);
    return data->elems[idx];
  }
  Cell *operator->() {
    VectorHandle *v = (VectorHandle *)cellPtr(vCell);
    VectorData *data = (VectorData *)cellPtr(v->dataPtr);
    return &data->elems[idx];
  }
  Cell &operator[](int64_t x) {
    VectorHandle *v = (VectorHandle *)cellPtr(vCell);
    VectorData *data = (VectorData *)cellPtr(v->dataPtr);
    return data->elems[idx + x];
  }
  VectorIter &operator++() {
    ++idx;
    return *this;
  }
  VectorIter &operator--() {
    --idx;
    return *this;
  }
  VectorIter operator++(int) {
    int64_t idx0 = idx++;
    return VectorIter(vCell, idx0);
  }
  VectorIter operator--(int) {
    int64_t idx0 = idx--;
    return VectorIter(vCell, idx0);
  }
  VectorIter &operator+=(int64_t x) {
    idx += x;
    return *this;
  }
  VectorIter &operator-=(int64_t x) {
    idx -= x;
    return *this;
  }
  friend VectorIter operator+(VectorIter &iter, int64_t x) {
    return VectorIter(iter.vCell, iter.idx + x);
  }
  friend VectorIter operator+(int64_t x, VectorIter &iter) {
    return VectorIter(iter.vCell, iter.idx + x);
  }
  friend VectorIter operator-(VectorIter &iter, int64_t x) {
    return VectorIter(iter.vCell, iter.idx - x);
  }
  friend int64_t operator-(VectorIter &iter1, VectorIter &iter2) {
    return (int64_t)(iter1.idx - iter2.idx);
  }
  friend bool operator==(VectorIter &iter1, VectorIter &iter2) {
    return iter1.idx == iter2.idx;
  }
  friend bool operator!=(VectorIter &iter1, VectorIter &iter2) {
    return iter1.idx != iter2.idx;
  }
  friend bool operator<(VectorIter &iter1, VectorIter &iter2) {
    return iter1.idx < iter2.idx;
  }
  friend bool operator<=(VectorIter &iter1, VectorIter &iter2) {
    return iter1.idx <= iter2.idx;
  }
  friend bool operator>(VectorIter &iter1, VectorIter &iter2) {
    return iter1.idx > iter2.idx;
  }
  friend bool operator>=(VectorIter &iter1, VectorIter &iter2) {
    return iter1.idx >= iter2.idx;
  }

  Cell &vCell;
  int64_t idx;
};

//------------------------------------------------------------------------

// Expand the vector in [vCell] to fit [newLength] elements. This may
// change the vector's size (capacity), but will not change its
// length; the caller is responsible for changing the vector length to
// [newLength]. This function may trigger GC.
static void vectorExpand(Cell &vCell, int64_t newLength, BytecodeEngine &engine) {
  VectorHandle *v = (VectorHandle *)cellPtr(vCell);
  engine.failOnNilPtr(v);
  VectorData *data = (VectorData *)cellPtr(v->dataPtr);
  int64_t length = heapObjSize(v);
  int64_t size = data ? heapObjSize(data) / bytesPerElement : 0;
  if (newLength <= size) {
    return;
  }

  int64_t newSize = size ? size : minVectorSize;
  while (newSize < newLength) {
    if (newSize > bytecodeMaxInt / 2) {
      BytecodeEngine::fatalError("Integer overflow");
    }
    newSize *= 2;
  }

  // NB: this may trigger GC
  VectorData *newData = (VectorData *)engine.heapAllocTuple(newSize, 0);

  v = (VectorHandle *)cellPtr(vCell);
  if (length > 0) {
    data = (VectorData *)cellPtr(v->dataPtr);
    memcpy(newData->elems, data->elems, length * bytesPerElement);
  }
  v->dataPtr = cellMakeHeapPtr(newData);
}

// Shrink the vector in [vCell] to fit its length. If the vector's
// size (capacity) is sufficiently larger than its length, the size
// will be reduced. This will not change the vector's length.  This
// function may trigger GC.
static void vectorShrink(Cell &vCell, BytecodeEngine &engine) {
  VectorHandle *v = (VectorHandle *)cellPtr(vCell);
  engine.failOnNilPtr(v);
  VectorData *data = (VectorData *)cellPtr(v->dataPtr);
  int64_t length = heapObjSize(v);
  int64_t size = data ? heapObjSize(data) / bytesPerElement : 0;
  if (size <= minVectorSize || size / 4 < length) {
    return;
  }

  int64_t newSize = size;
  do {
    newSize /= 2;
  } while (newSize / 4 >= length && newSize > minVectorSize);

  // NB: this may trigger GC
  VectorData *newData = (VectorData *)engine.heapAllocTuple(newSize, 0);

  v = (VectorHandle *)cellPtr(vCell);
  if (length > 0) {
    data = (VectorData *)cellPtr(v->dataPtr);
    memcpy(newData->elems, data->elems, length * bytesPerElement);
  }
  v->dataPtr = cellMakeHeapPtr(newData);
}

static NativeFuncDefn(runtime_allocVector) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 0) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  engine.push(vectorMake(engine));
}

// length(v: Vector[$T]) -> Int
static NativeFuncDefn(runtime_length_V) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &vCell = engine.arg(0);

  VectorHandle *v = (VectorHandle *)cellPtr(vCell);
  engine.failOnNilPtr(v);
  int64_t length = heapObjSize(v);

  engine.push(cellMakeInt(length));
}

// get(v: Vector[$T], idx: Int) -> $T
// iget(v: Vector[$T], iter: Int) -> $T
static NativeFuncDefn(runtime_get_V) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &vCell = engine.arg(0);
  Cell &idxCell = engine.arg(1);

  VectorHandle *v = (VectorHandle *)cellPtr(vCell);
  engine.failOnNilPtr(v);
  int64_t idx = cellInt(idxCell);

  int64_t length = heapObjSize(v);
  if (idx < 0 || idx >= length) {
    BytecodeEngine::fatalError("Index out of bounds");
  }
  VectorData *data = (VectorData *)cellPtr(v->dataPtr);
  Cell value = data->elems[idx];
  engine.push(value);
}

// set(v: Vector[$T], idx: Int, value: $T)
static NativeFuncDefn(runtime_set_V) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 3 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &vCell = engine.arg(0);
  Cell &idxCell = engine.arg(1);
  Cell &valueCell = engine.arg(2);

  VectorHandle *v = (VectorHandle *)cellPtr(vCell);
  engine.failOnNilPtr(v);
  int64_t idx = cellInt(idxCell);

  int64_t length = heapObjSize(v);
  if (idx < 0 || idx >= length) {
    BytecodeEngine::fatalError("Index out of bounds");
  }
  VectorData *data = (VectorData *)cellPtr(v->dataPtr);
  data->elems[idx] = valueCell;

  engine.push(cellMakeInt(0));
}

// append(v: Vector[$T], value: $T)
static NativeFuncDefn(runtime_append_V) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &vCell = engine.arg(0);
  Cell &valueCell = engine.arg(1);

  vectorAppend(vCell, valueCell, engine);

  engine.push(cellMakeInt(0));
}

// insert(v: Vector[$T], idx: Int, value: $T)
static NativeFuncDefn(runtime_insert_V) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 3 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &vCell = engine.arg(0);
  Cell &idxCell = engine.arg(1);
  Cell &valueCell = engine.arg(2);

  VectorHandle *v = (VectorHandle *)cellPtr(vCell);
  engine.failOnNilPtr(v);
  int64_t idx = cellInt(idxCell);

  int64_t length = heapObjSize(v);
  if (idx < 0 || idx > length) {
    BytecodeEngine::fatalError("Index out of bounds");
  }
  if (length > bytecodeMaxInt - 1) {
    BytecodeEngine::fatalError("Integer overflow");
  }

  // NB: this may trigger GC
  vectorExpand(vCell, length + 1, engine);

  v = (VectorHandle *)cellPtr(vCell);
  VectorData *data = (VectorData *)cellPtr(v->dataPtr);
  if (idx < length) {
    memmove(&data->elems[idx+1], &data->elems[idx], (length - idx) * bytesPerElement);
  }
  data->elems[idx] = valueCell;
  heapObjSetSize(v, length + 1);

  engine.push(cellMakeInt(0));
}

// delete(v: Vector[$T], idx: Int)
static NativeFuncDefn(runtime_delete_V) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &vCell = engine.arg(0);
  Cell &idxCell = engine.arg(1);

  VectorHandle *v = (VectorHandle *)cellPtr(vCell);
  engine.failOnNilPtr(v);
  int64_t idx = cellInt(idxCell);

  int64_t length = heapObjSize(v);
  if (idx < 0 || idx >= length) {
    BytecodeEngine::fatalError("Index out of bounds");
  }

  // elems[0]  elems[1]  ...  elems[idx]  elems[idx+1]  ...  elems[length-1]
  VectorData *data = (VectorData *)cellPtr(v->dataPtr);
  if (idx < length - 1) {
    memmove(&data->elems[idx], &data->elems[idx+1], (length - 1 - idx) * bytesPerElement);
  }
  heapObjSetSize(v, length - 1);

  // NB: this may trigger GC
  vectorShrink(vCell, engine);

  engine.push(cellMakeInt(0));
}

// clear(v: Vector[$T])
static NativeFuncDefn(runtime_clear_V) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &vCell = engine.arg(0);

  VectorHandle *v = (VectorHandle *)cellPtr(vCell);
  engine.failOnNilPtr(v);
  v->dataPtr = cellMakeNilHeapPtr();
  heapObjSetSize(v, 0);

  engine.push(cellMakeInt(0));
}

// sort(v: Vector[$T], cmp: Func[$T,$T->Bool])
static NativeFuncDefn(runtime_sort_V) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &vCell = engine.arg(0);
  Cell &cmpCell = engine.arg(1);

  VectorHandle *v = (VectorHandle *)cellPtr(vCell);
  engine.failOnNilPtr(v);
  int64_t length = heapObjSize(v);

  std::sort(VectorIter(vCell, 0), VectorIter(vCell, length),
	    [&engine, &cmpCell](Cell &cell1, Cell &cell2) {
	      engine.push(cell1);
	      engine.push(cell2);
	      engine.callFunctionPtr(cmpCell, 2);
	      return engine.popBool();
	    });

  engine.push(cellMakeInt(0));
}

// ifirst(v: Vector[$T]) -> Int
static NativeFuncDefn(runtime_ifirst_V) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &vCell = engine.arg(0);

  engine.push(cellMakeInt(0));
}

// imore(v: Vector[$T], iter: Int) -> Bool
static NativeFuncDefn(runtime_imore_V) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &vCell = engine.arg(0);
  Cell &idxCell = engine.arg(1);

  int64_t idx = cellInt(idxCell);
  VectorHandle *v = (VectorHandle *)cellPtr(vCell);
  engine.failOnNilPtr(v);
  int64_t length = heapObjSize(v);
  engine.push(cellMakeBool(idx < length));
}

// inext(v: Vector[$T], iter: Int) -> Int
static NativeFuncDefn(runtime_inext_V) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &vCell = engine.arg(0);
  Cell &idxCell = engine.arg(1);

  int64_t idx = cellInt(idxCell);
  VectorHandle *v = (VectorHandle *)cellPtr(vCell);
  engine.failOnNilPtr(v);
  int64_t length = heapObjSize(v);
  engine.push(cellMakeInt(idx < length ? idx + 1 : idx));
}

void runtime_Vector_init(BytecodeEngine &engine) {
  engine.addNativeFunction("_allocVector", &runtime_allocVector);
  engine.addNativeFunction("length_V", &runtime_length_V);
  engine.addNativeFunction("get_V", &runtime_get_V);
  engine.addNativeFunction("set_V", &runtime_set_V);
  engine.addNativeFunction("append_V", &runtime_append_V);
  engine.addNativeFunction("insert_V", &runtime_insert_V);
  engine.addNativeFunction("delete_V", &runtime_delete_V);
  engine.addNativeFunction("clear_V", &runtime_clear_V);
  engine.addNativeFunction("sort_V", &runtime_sort_V);
  engine.addNativeFunction("ifirst_V", &runtime_ifirst_V);
  engine.addNativeFunction("imore_V", &runtime_imore_V);
  engine.addNativeFunction("inext_V", &runtime_inext_V);
  engine.addNativeFunction("iget_V", &runtime_get_V);
}

//------------------------------------------------------------------------
// support functions
//------------------------------------------------------------------------

Cell vectorMake(BytecodeEngine &engine) {
  VectorHandle *v = (VectorHandle *)engine.heapAllocHandle(0, 0);
  v->dataPtr = cellMakeNilHeapPtr();
  return cellMakeHeapPtr(v);
}

int64_t vectorLength(Cell &vCell) {
  VectorHandle *v = (VectorHandle *)cellPtr(vCell);
  BytecodeEngine::failOnNilPtr(v);
  return heapObjSize(v);
}

Cell vectorGet(Cell &vCell, int64_t idx) {
  VectorHandle *v = (VectorHandle *)cellPtr(vCell);
  BytecodeEngine::failOnNilPtr(v);
  int64_t length = heapObjSize(v);
  if (idx < 0 || idx >= length) {
    BytecodeEngine::fatalError("Index out of bounds");
  }
  VectorData *data = (VectorData *)cellPtr(v->dataPtr);
  return data->elems[idx];
}

void vectorAppend(Cell &vCell, Cell &elemCell, BytecodeEngine &engine) {
  VectorHandle *v = (VectorHandle *)cellPtr(vCell);
  engine.failOnNilPtr(v);
  int64_t length = heapObjSize(v);
  if (length > bytecodeMaxInt - 1) {
    BytecodeEngine::fatalError("Integer overflow");
  }

  // NB: this may trigger GC
  vectorExpand(vCell, length + 1, engine);

  v = (VectorHandle *)cellPtr(vCell);
  VectorData *data = (VectorData *)cellPtr(v->dataPtr);
  data->elems[length] = elemCell;
  heapObjSetSize(v, length + 1);
}
