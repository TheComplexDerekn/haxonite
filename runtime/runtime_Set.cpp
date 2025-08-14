//========================================================================
//
// runtime_Set.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

// Sets are stored as hash tables. An array of 'first' pointers point
// to the heads of linked lists of entries for each hash
// value. Entries consist of a key and a 'next' pointer.
//
// +---------------+     +-----+------+     +-----+------+
// | first[0]      | --> | key | next | --> | key | next | --X
// +---------------+     +-----+------+     +-----+------+
// | first[1]      | --> ...
// +---------------+
// | ...           |
// +---------------+
// | first[size-1] | --> ...
// +---------------+
//
// Entries (key/next pairs) are preallocated to avoid requiring a
// memory allocation for each insert. The max load factor is 1, so the
// number of entries is the same as the number of 'first' pointers.
//
// The first pointers and entries are stored in an interleaved array.
// (The fact that they're the same length allows this; and it makes
// defining the variable-size struct easier.) But they are treated as
// two completely independent arrays.
// 
// There is a linked list of free entries, using the next pointers to
// link them.
// 
// A set is implemented a handle that points to a tuple of cells,
// which contains the free list pointer and the bucket array (i.e.,
// the two interleaved arrays).
//
// +--------+---------+
// | length | pointer |
// +--------+---------+
// handle        |
//               v
//          +---------+------+----------+--------+---------+--
//          | size*24 | free | first[0] | key[0] | next[0] |
//          +---------+------+----------+--------+---------+--
//          tuple      --+----------+--------+---------+--
//                       | first[1] | key[1] | next[1] |
//                     --+----------+--------+---------+--
//                     ...
//                     --+---------------+-------------+--------------+
//                       | first[size-1] | key[size-1] | next[size-1] |
//                     --+---------------+-------------+--------------+
//
// - The length field in the handle is the number of elements in
//   the set.
// - The size field in the bucket array tuple is the number of bytes
//   in the tuple, i.e., 8 * 3 * the number of hash table buckets.
// - Bucket and next pointers are stored as integer indexes (to avoid
//   pointers into the middle of objects).
// - Empty buckets and the end of node chains are indicated by
//   pointer=size.
//
// As a special case, an empty set is a handle with a nil pointer:
//
// +----------+-------------+
// | length=0 | pointer=nil |
// +----------+-------------+

#include "runtime_Set.h"
#include "BytecodeDefs.h"
#include "Hash.h"
#include "runtime_String.h"

//------------------------------------------------------------------------

#define minSetSize 8

struct SetHandle {
  uint64_t hdr;
  Cell arrayPtr;
};

struct SetBucket {
  Cell first;
  Cell key;
  Cell next;
};
#define bytesPerBucket 24

struct SetArray {
  uint64_t hdr;
  Cell free;
  SetBucket buckets[0];
};

//------------------------------------------------------------------------

static int64_t doHashString(Cell &cell, int64_t size) {
  return hashFold(hashString(cell), size);
}

static int64_t doHashInt(Cell &cell, int64_t size) {
  return hashFold(hashInt(cellInt(cell)), size);
}

static bool doCompareStrings(Cell &cell1, Cell &cell2) {
  return stringCompare(cell1, cell2) == 0;
}

static bool doCompareInts(Cell &cell1, Cell &cell2) {
  return cellInt(cell1) == cellInt(cell2);
}

// Initialize [newData] to empty, and move all hash table elements
// from [data] to [newData].
static void rehash(SetArray *array, int64_t size, SetArray *newArray, int64_t newSize,
		   int64_t (*doHash)(Cell &cell, int64_t size)) {
  // initialize all of the first pointers to nil
  for (int64_t i = 0; i < newSize; ++i) {
    newArray->buckets[i].first = cellMakeInt(newSize);
  }

  // copy all of the entries
  int64_t newLength = 0;
  for (int64_t i = 0; i < size; ++i) {
    if (!cellIsNilHeapPtr(array->buckets[i].key)) {
      int64_t h = (*doHash)(array->buckets[i].key, newSize);
      newArray->buckets[newLength].key = array->buckets[i].key;
      newArray->buckets[newLength].next = newArray->buckets[h].first;
      newArray->buckets[h].first = cellMakeInt(newLength);
      ++newLength;
    }
  }

  // construct the free list
  for (int64_t i = newLength; i < newSize; ++i) {
    newArray->buckets[i].key = cellMakeNilHeapPtr();
    newArray->buckets[i].next = cellMakeInt(i == newSize - 1 ? newSize : i+1);
  }
  newArray->free = cellMakeInt(newLength);
}

// Expand the set in [sCell] to fit [newLength] elements. This may
// change the set's size (number of hash table buckets), but will not
// change the set's length; the caller is responsible for changing the
// set's length to [newLength]. This function may trigger GC. Returns
// true if the set was expanded.
static bool setExpand(Cell &sCell, int64_t newLength,
		      int64_t (*doHash)(Cell &cell, int64_t size),
		      BytecodeEngine &engine) {
  SetHandle *s = (SetHandle *)cellPtr(sCell);
  engine.failOnNilPtr(s);
  SetArray *array = (SetArray *)cellPtr(s->arrayPtr);
  int64_t length = heapObjSize(s);
  int64_t size = array ? heapObjSize(array) / bytesPerBucket : 0;
  if (newLength <= size) {
    return false;
  }

  int64_t newSize = size ? size : minSetSize;
  while (newSize < newLength) {
    if (newSize > (bytecodeMaxInt - 1) / 6) {
      BytecodeEngine::fatalError("Integer overflow");
    }
    newSize *= 2;
  }

  // NB: this may trigger GC
  SetArray *newArray = (SetArray *)engine.heapAllocTuple(1 + 3 * newSize, 0);

  s = (SetHandle *)cellPtr(sCell);
  array = (SetArray *)cellPtr(s->arrayPtr);
  rehash(array, size, newArray, newSize, doHash);
  s->arrayPtr = cellMakeHeapPtr(newArray);

  return true;
}

// Shrink the set in [sCell] to fit its length. If the set's size
// (number of hash table buckets) is sufficiently larger than its
// length, the size will be reduced. This will not change the set's
// length. This function may trigger GC. Returns true if the set was
// shrunk.
static bool setShrink(Cell &sCell,
		      int64_t (*doHash)(Cell &cell, int64_t size),
		      BytecodeEngine &engine) {
  SetHandle *s = (SetHandle *)cellPtr(sCell);
  engine.failOnNilPtr(s);
  SetArray *array = (SetArray *)cellPtr(s->arrayPtr);
  int64_t length = heapObjSize(s);
  int64_t size = array ? heapObjSize(array) / bytesPerBucket : 0;
  if (size <= minSetSize || size / 4 < length) {
    return false;
  }

  int64_t newSize = size;
  do {
    newSize /= 2;
  } while (newSize > minSetSize && newSize / 4 >= length);

  // NB: this may trigger GC
  SetArray *newArray = (SetArray *)engine.heapAllocTuple(1 + 3 * newSize, 0);

  s = (SetHandle *)cellPtr(sCell);
  array = (SetArray *)cellPtr(s->arrayPtr);
  rehash(array, size, newArray, newSize, doHash);
  s->arrayPtr = cellMakeHeapPtr(newArray);

  return true;
}

static NativeFuncDefn(runtime_allocSet) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 0) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  SetHandle *s = (SetHandle *)engine.heapAllocHandle(0, 0);
  s->arrayPtr = cellMakeNilHeapPtr();
  engine.push(cellMakeHeapPtr(s));
}

// length(s: Set[$K]) -> Int
static NativeFuncDefn(runtime_length_Z1) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);

  SetHandle *s = (SetHandle *)cellPtr(sCell);
  engine.failOnNilPtr(s);
  int64_t length = heapObjSize(s);

  engine.push(cellMakeInt(length));
}

static void doContains(Cell &sCell, Cell &elemCell,
		       int64_t (*doHash)(Cell &cell, int64_t size),
		       bool (*doCompare)(Cell &cell1, Cell &cell2),
		       BytecodeEngine &engine) {
  SetHandle *s = (SetHandle *)cellPtr(sCell);
  engine.failOnNilPtr(s);
  int64_t length = heapObjSize(s);

  bool result = false;
  if (length > 0) {
    SetArray *array = (SetArray *)cellPtr(s->arrayPtr);
    int64_t size = heapObjSize(array) / bytesPerBucket;
    int64_t h = (*doHash)(elemCell, size);
    int64_t idx = cellInt(array->buckets[h].first);
    while (idx < size) {
      if ((*doCompare)(array->buckets[idx].key, elemCell)) {
	result = true;
	break;
      }
      idx = cellInt(array->buckets[idx].next);
    }
  }

  engine.push(cellMakeBool(result));
}

// contains(s: Set[String], elem: String) -> Bool
static NativeFuncDefn(runtime_contains_ZS2) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);
  Cell &elemCell = engine.arg(1);
  BytecodeEngine::failOnNilPtr(elemCell);
  doContains(sCell, elemCell, &doHashString, &doCompareStrings, engine);
}

// contains(s: Set[Int], elem: Int) -> Bool
static NativeFuncDefn(runtime_contains_ZI2) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);
  Cell &elemCell = engine.arg(1);
  doContains(sCell, elemCell, &doHashInt, &doCompareInts, engine);
}

static void doInsert(Cell &sCell, Cell &elemCell,
		     int64_t (*doHash)(Cell &cell, int64_t size),
		     bool (*doCompare)(Cell &cell1, Cell &cell2),
		     BytecodeEngine &engine) {
  SetHandle *s = (SetHandle *)cellPtr(sCell);
  engine.failOnNilPtr(s);
  SetArray *array = (SetArray *)cellPtr(s->arrayPtr);
  int64_t length = heapObjSize(s);
  int64_t size = array ? heapObjSize(array) / bytesPerBucket : 0;

  // search for the element
  int64_t h = 0;
  int64_t idx;
  if (array) {
    h = (*doHash)(elemCell, size);
    idx = cellInt(array->buckets[h].first);
    while (idx < size) {
      if ((*doCompare)(array->buckets[idx].key, elemCell)) {
	break;
      }
      idx = cellInt(array->buckets[idx].next);
    }
  } else {
    idx = size;
  }

  // if not found: insert it
  if (idx == size) {

    // NB: this may trigger GC
    if (setExpand(sCell, length + 1, doHash, engine)) {
      s = (SetHandle *)cellPtr(sCell);
      array = (SetArray *)cellPtr(s->arrayPtr);
      size = heapObjSize(array) / bytesPerBucket;
      h = (*doHash)(elemCell, size);
    }

    // remove the next node from the free list
    int64_t free = cellInt(array->free);
    array->free = array->buckets[free].next;

    // initialize the new node
    array->buckets[free].key = elemCell;
    array->buckets[free].next = array->buckets[h].first;
    array->buckets[h].first = cellMakeInt(free);

    heapObjSetSize(s, length + 1);
  }

  engine.push(cellMakeInt(0));
}

// insert(s: Set[String], elem: String)
static NativeFuncDefn(runtime_insert_ZS2) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);
  Cell &elemCell = engine.arg(1);
  BytecodeEngine::failOnNilPtr(elemCell);
  doInsert(sCell, elemCell, &doHashString, &doCompareStrings, engine);
}

// insert(s: Set[Int], elem: Int)
static NativeFuncDefn(runtime_insert_ZI2) {
  if (engine.nArgs() != 2) {
    BytecodeEngine::fatalError("Invalid argument");
  }
  Cell &sCell = engine.arg(0);
  Cell &elemCell = engine.arg(1);
  if (!cellIsPtr(sCell) || !cellIsInt(elemCell)) {
    BytecodeEngine::fatalError("Invalid argument");
  }
  doInsert(sCell, elemCell, &doHashInt, &doCompareInts, engine);
}

static void doDelete(Cell &sCell, Cell &elemCell,
		     int64_t (*doHash)(Cell &cell, int64_t size),
		     bool (*doCompare)(Cell &cell1, Cell &cell2),
		     BytecodeEngine &engine) {
  SetHandle *s = (SetHandle *)cellPtr(sCell);
  engine.failOnNilPtr(s);
  SetArray *array = (SetArray *)cellPtr(s->arrayPtr);
  int64_t length = heapObjSize(s);
  int64_t size = array ? heapObjSize(array) / bytesPerBucket : 0;

  if (length > 0) {
    int64_t h = (*doHash)(elemCell, size);
    int64_t prevIdx = size;
    int64_t idx = cellInt(array->buckets[h].first);
    while (idx < size) {
      if ((*doCompare)(array->buckets[idx].key, elemCell)) {
	break;
      }
      prevIdx = idx;
      idx = cellInt(array->buckets[idx].next);
    }
    if (idx < size) {

      // remove the node from the chain
      if (prevIdx < size) {
	array->buckets[prevIdx].next = array->buckets[idx].next;
      } else {
	array->buckets[h].first = array->buckets[idx].next;
      }

      // add the node to the free list
      array->buckets[idx].key = cellMakeNilHeapPtr();
      array->buckets[idx].next = array->free;
      array->free = cellMakeInt(idx);

      // reduce length and shrink the array if needed
      heapObjSetSize(s, length - 1);
      setShrink(sCell, doHash, engine);
    }
  }

  engine.push(cellMakeInt(0));
}

// delete(s: Set[String], elem: String)
static NativeFuncDefn(runtime_delete_ZS2) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);
  Cell &elemCell = engine.arg(1);
  BytecodeEngine::failOnNilPtr(elemCell);
  doDelete(sCell, elemCell, &doHashString, &doCompareStrings, engine);
}

// delete(s: Set[Int], elem: Int)
static NativeFuncDefn(runtime_delete_ZI2) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);
  Cell &elemCell = engine.arg(1);
  doDelete(sCell, elemCell, &doHashInt, &doCompareInts, engine);
}

// clear(s: Set[$K])
static NativeFuncDefn(runtime_clear_Z1) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);

  SetHandle *s = (SetHandle *)cellPtr(sCell);
  engine.failOnNilPtr(s);
  s->arrayPtr = cellMakeNilHeapPtr();
  heapObjSetSize(s, 0);

  engine.push(cellMakeInt(0));
}

// ifirst(s: Set[$K]) -> Int
static NativeFuncDefn(runtime_ifirst_Z1) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);

  SetHandle *s = (SetHandle *)cellPtr(sCell);
  engine.failOnNilPtr(s);
  SetArray *array = (SetArray *)cellPtr(s->arrayPtr);
  int64_t size = array ? (heapObjSize(array) / bytesPerBucket) : 0;
  int64_t iter;
  for (iter = 0; iter < size && cellIsNilHeapPtr(array->buckets[iter].key); ++iter) ;

  engine.push(cellMakeInt(iter));
}

// imore(s: Set[$K], iter: Int) -> Bool
static NativeFuncDefn(runtime_imore_Z2) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);
  Cell &iterCell = engine.arg(1);

  SetHandle *s = (SetHandle *)cellPtr(sCell);
  engine.failOnNilPtr(s);
  SetArray *array = (SetArray *)cellPtr(s->arrayPtr);
  int64_t size = array ? (heapObjSize(array) / bytesPerBucket) : 0;
  int64_t iter = cellInt(iterCell);

  engine.push(cellMakeBool(iter < size));
}

// inext(s: Set[$K], iter: Int) -> Int
static NativeFuncDefn(runtime_inext_Z2) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);
  Cell &iterCell = engine.arg(1);

  SetHandle *s = (SetHandle *)cellPtr(sCell);
  engine.failOnNilPtr(s);
  SetArray *array = (SetArray *)cellPtr(s->arrayPtr);
  int64_t size = array ? (heapObjSize(array) / bytesPerBucket) : 0;
  int64_t iter = cellInt(iterCell);
  if (iter < size) {
    for (++iter; iter < size && cellIsNilHeapPtr(array->buckets[iter].key); ++iter) ;
  }

  engine.push(cellMakeInt(iter));
}

// iget(s: Set[$K], iter: Int) -> $K
static NativeFuncDefn(runtime_iget_Z2) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &sCell = engine.arg(0);
  Cell &iterCell = engine.arg(1);

  SetHandle *s = (SetHandle *)cellPtr(sCell);
  engine.failOnNilPtr(s);
  SetArray *array = (SetArray *)cellPtr(s->arrayPtr);
  int64_t size = array ? (heapObjSize(array) / bytesPerBucket) : 0;
  int64_t iter = cellInt(iterCell);
  if (iter < 0 || iter >= size) {
    BytecodeEngine::fatalError("Index out of bounds");
  }

  engine.push(array->buckets[iter].key);
}

void runtime_Set_init(BytecodeEngine &engine) {
  engine.addNativeFunction("_allocSet", &runtime_allocSet);

  engine.addNativeFunction("length_ZS1", &runtime_length_Z1);
  engine.addNativeFunction("contains_ZS2", &runtime_contains_ZS2);
  engine.addNativeFunction("insert_ZS2", &runtime_insert_ZS2);
  engine.addNativeFunction("delete_ZS2", &runtime_delete_ZS2);
  engine.addNativeFunction("clear_ZS1", &runtime_clear_Z1);
  engine.addNativeFunction("ifirst_ZS1", &runtime_ifirst_Z1);
  engine.addNativeFunction("imore_ZS2", &runtime_imore_Z2);
  engine.addNativeFunction("inext_ZS2", &runtime_inext_Z2);
  engine.addNativeFunction("iget_ZS2", &runtime_iget_Z2);

  engine.addNativeFunction("length_ZI1", &runtime_length_Z1);
  engine.addNativeFunction("contains_ZI2", &runtime_contains_ZI2);
  engine.addNativeFunction("insert_ZI2", &runtime_insert_ZI2);
  engine.addNativeFunction("delete_ZI2", &runtime_delete_ZI2);
  engine.addNativeFunction("clear_ZI1", &runtime_clear_Z1);
  engine.addNativeFunction("ifirst_ZI1", &runtime_ifirst_Z1);
  engine.addNativeFunction("imore_ZI2", &runtime_imore_Z2);
  engine.addNativeFunction("inext_ZI2", &runtime_inext_Z2);
  engine.addNativeFunction("iget_ZI2", &runtime_iget_Z2);
}
