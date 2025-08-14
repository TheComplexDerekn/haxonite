//========================================================================
//
// runtime_Map.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

// Maps are stored as hash tables. An array of 'first' pointers point
// to the heads of linked lists of entries for each hash
// value. Entries consist of a key, value, and 'next' pointer.
//
// +---------------+     +-----+-----+------+     +-----+-----+------+
// | first[0]      | --> | key | val | next | --> | key | val | next | --X
// +---------------+     +-----+-----+------+     +-----+-----+------+
// | first[1]      | --> ...
// +---------------+
// | ...           |
// +---------------+
// | first[size-1] | --> ...
// +---------------+
//
// Entries (key/val/next triples) are preallocated to avoid requiring
// a memory allocation for each insert. The max load factor is 1, so
// the number of entries is the same as the number of 'first'
// pointers.
//
// The first pointers and entries are stored in an interleaved array.
// (The fact that they're the same length allows this; and it makes
// defining the variable-size struct easier.) But they are treated as
// two completely independent arrays.
// 
// There is a linked list of free entries, using the next pointers to
// link them.
// 
// A map is implemented a handle that points to a tuple of cells,
// which contains the free list pointer and the bucket array (i.e.,
// the two interleaved arrays).
//
// +--------+---------+
// | length | pointer |
// +--------+---------+
// handle        |
//               v
//          +---------+------+----------+--------+--------+---------+--
//          | size*32 | free | first[0] | key[0] | val[0] | next[0] |
//          +---------+------+----------+--------+--------+---------+--
//          tuple      --+----------+--------+--------+---------+--
//                       | first[1] | key[1] | val[1] | next[1] |
//                     --+----------+--------+--------+---------+--
//                     ...
//                     --+---------------+-------------+-------------+--------------+
//                       | first[size-1] | key[size-1] | val[size-1] | next[size-1] |
//                     --+---------------+-------------+-------------+--------------+
//
// - The length field in the handle is the number of elements in
//   the map.
// - The size field in the bucket array tuple is the number of bytes
//   in the tuple, i.e., 8 * 4 * the number of hash table buckets.
// - Bucket and next pointers are stored as integer indexes (to avoid
//   pointers into the middle of objects).
// - Empty buckets and the end of node chains are indicated by
//   pointer=size.
//
// As a special case, an empty map is a handle with a nil pointer:
//
// +----------+-------------+
// | length=0 | pointer=nil |
// +----------+-------------+

#include "runtime_Map.h"
#include "BytecodeDefs.h"
#include "Hash.h"
#include "runtime_String.h"

//------------------------------------------------------------------------

#define minMapSize 8

struct MapHandle {
  uint64_t hdr;
  Cell arrayPtr;
};

struct MapBucket {
  Cell first;
  Cell key;
  Cell val;
  Cell next;
};
#define bytesPerBucket 32

struct MapArray {
  uint64_t hdr;
  Cell free;
  MapBucket buckets[0];
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
static void rehash(MapArray *array, int64_t size, MapArray *newArray, int64_t newSize,
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
      newArray->buckets[newLength].val = array->buckets[i].val;
      newArray->buckets[newLength].next = newArray->buckets[h].first;
      newArray->buckets[h].first = cellMakeInt(newLength);
      ++newLength;
    }
  }

  // construct the free list
  for (int64_t i = newLength; i < newSize; ++i) {
    newArray->buckets[i].key = cellMakeNilHeapPtr();
    newArray->buckets[i].val = cellMakeNilHeapPtr();
    newArray->buckets[i].next = cellMakeInt(i == newSize - 1 ? newSize : i+1);
  }
  newArray->free = cellMakeInt(newLength);
}

// Expand the map in [mCell] to fit [newLength] elements. This may
// change the map's size (number of hash table buckets), but will not
// change the map's length; the caller is responsible for changing the
// map's length to [newLength]. This function may trigger GC. Returns
// true if the map was expanded.
static bool mapExpand(Cell &mCell, int64_t newLength,
		      int64_t (*doHash)(Cell &cell, int64_t size),
		      BytecodeEngine &engine) {
  MapHandle *m = (MapHandle *)cellPtr(mCell);
  engine.failOnNilPtr(m);
  MapArray *array = (MapArray *)cellPtr(m->arrayPtr);
  int64_t length = heapObjSize(m);
  int64_t size = array ? heapObjSize(array) / bytesPerBucket : 0;
  if (newLength <= size) {
    return false;
  }

  int64_t newSize = size ? size : minMapSize;
  while (newSize < newLength) {
    if (newSize > (bytecodeMaxInt - 1) / 6) {
      BytecodeEngine::fatalError("Integer overflow");
    }
    newSize *= 2;
  }

  // NB: this may trigger GC
  MapArray *newArray = (MapArray *)engine.heapAllocTuple(1 + 4 * newSize, 0);

  m = (MapHandle *)cellPtr(mCell);
  array = (MapArray *)cellPtr(m->arrayPtr);
  rehash(array, size, newArray, newSize, doHash);
  m->arrayPtr = cellMakeHeapPtr(newArray);

  return true;
}

// Shrink the map in [mCell] to fit its length. If the map's size
// (number of hash table buckets) is sufficiently larger than its
// length, the size will be reduced. This will not change the map's
// length. This function may trigger GC. Returns true if the map was
// shrunk.
static bool mapShrink(Cell &mCell,
		      int64_t (*doHash)(Cell &cell, int64_t size),
		      BytecodeEngine &engine) {
  MapHandle *m = (MapHandle *)cellPtr(mCell);
  engine.failOnNilPtr(m);
  MapArray *array = (MapArray *)cellPtr(m->arrayPtr);
  int64_t length = heapObjSize(m);
  int64_t size = array ? heapObjSize(array) / bytesPerBucket : 0;
  if (size <= minMapSize || size / 4 < length) {
    return false;
  }

  int64_t newSize = size;
  do {
    newSize /= 2;
  } while (newSize > minMapSize && newSize / 4 >= length);

  // NB: this may trigger GC
  MapArray *newArray = (MapArray *)engine.heapAllocTuple(1 + 4 * newSize, 0);

  m = (MapHandle *)cellPtr(mCell);
  array = (MapArray *)cellPtr(m->arrayPtr);
  rehash(array, size, newArray, newSize, doHash);
  m->arrayPtr = cellMakeHeapPtr(newArray);

  return true;
}

static NativeFuncDefn(runtime_allocMap) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 0) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  MapHandle *m = (MapHandle *)engine.heapAllocHandle(0, 0);
  m->arrayPtr = cellMakeNilHeapPtr();
  engine.push(cellMakeHeapPtr(m));
}

// length(m: Map[$K:$T]) -> Int
static NativeFuncDefn(runtime_length_M1) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &mCell = engine.arg(0);

  MapHandle *m = (MapHandle *)cellPtr(mCell);
  engine.failOnNilPtr(m);
  int64_t length = heapObjSize(m);

  engine.push(cellMakeInt(length));
}

static void doContains(Cell &mCell, Cell &keyCell,
		       int64_t (*doHash)(Cell &cell, int64_t size),
		       bool (*doCompare)(Cell &cell1, Cell &cell2),
		       BytecodeEngine &engine) {
  MapHandle *m = (MapHandle *)cellPtr(mCell);
  engine.failOnNilPtr(m);
  int64_t length = heapObjSize(m);

  bool result = false;
  if (length > 0) {
    MapArray *array = (MapArray *)cellPtr(m->arrayPtr);
    int64_t size = heapObjSize(array) / bytesPerBucket;
    int64_t h = (*doHash)(keyCell, size);
    int64_t idx = cellInt(array->buckets[h].first);
    while (idx < size) {
      if ((*doCompare)(array->buckets[idx].key, keyCell)) {
	result = true;
	break;
      }
      idx = cellInt(array->buckets[idx].next);
    }
  }

  engine.push(cellMakeBool(result));
}

// contains(m: Map[String:$T], key: String) -> Bool
static NativeFuncDefn(runtime_contains_MS2) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &mCell = engine.arg(0);
  Cell &keyCell = engine.arg(1);
  BytecodeEngine::failOnNilPtr(keyCell);
  doContains(mCell, keyCell, &doHashString, &doCompareStrings, engine);
}

// contains(m: Map[Int:$T], key: Int) -> Bool
static NativeFuncDefn(runtime_contains_MI2) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &mCell = engine.arg(0);
  Cell &keyCell = engine.arg(1);
  doContains(mCell, keyCell, &doHashInt, &doCompareInts, engine);
}

static void doGet(Cell &mCell, Cell &keyCell,
		  int64_t (*doHash)(Cell &cell, int64_t size),
		  bool (*doCompare)(Cell &cell1, Cell &cell2),
		  BytecodeEngine &engine) {
  MapHandle *m = (MapHandle *)cellPtr(mCell);
  engine.failOnNilPtr(m);
  int64_t length = heapObjSize(m);

  bool result = false;
  if (length > 0) {
    MapArray *array = (MapArray *)cellPtr(m->arrayPtr);
    int64_t size = heapObjSize(array) / bytesPerBucket;
    int64_t h = (*doHash)(keyCell, size);
    int64_t idx = cellInt(array->buckets[h].first);
    while (idx < size) {
      if ((*doCompare)(array->buckets[idx].key, keyCell)) {
	engine.push(array->buckets[idx].val);
	return;
      }
      idx = cellInt(array->buckets[idx].next);
    }
  }

  BytecodeEngine::fatalError("Index out of bounds");
}

// get(m: Map[String:$T], key: String) -> $T
static NativeFuncDefn(runtime_get_MS2) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &mCell = engine.arg(0);
  Cell &keyCell = engine.arg(1);
  BytecodeEngine::failOnNilPtr(keyCell);
  doGet(mCell, keyCell, &doHashString, &doCompareStrings, engine);
}

// get(m: Map[Int:$T], key: Int) -> $T
static NativeFuncDefn(runtime_get_MI2) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &mCell = engine.arg(0);
  Cell &keyCell = engine.arg(1);
  doGet(mCell, keyCell, &doHashInt, &doCompareInts, engine);
}

static void doSet(Cell &mCell, Cell &keyCell, Cell &valueCell,
		  int64_t (*doHash)(Cell &cell, int64_t size),
		  bool (*doCompare)(Cell &cell1, Cell &cell2),
		  BytecodeEngine &engine) {
  MapHandle *m = (MapHandle *)cellPtr(mCell);
  engine.failOnNilPtr(m);
  MapArray *array = (MapArray *)cellPtr(m->arrayPtr);
  int64_t length = heapObjSize(m);
  int64_t size = array ? heapObjSize(array) / bytesPerBucket : 0;

  // search for the key
  int64_t h = 0;
  int64_t idx;
  if (array) {
    h = (*doHash)(keyCell, size);
    idx = cellInt(array->buckets[h].first);
    while (idx < size) {
      if ((*doCompare)(array->buckets[idx].key, keyCell)) {
	break;
      }
      idx = cellInt(array->buckets[idx].next);
    }
  } else {
    idx = size;
  }

  // if found: set the value
  if (idx < size) {
    array->buckets[idx].val = valueCell;

  // if not found: insert a new element
  } else {

    // NB: this may trigger GC
    if (mapExpand(mCell, length + 1, doHash, engine)) {
      m = (MapHandle *)cellPtr(mCell);
      array = (MapArray *)cellPtr(m->arrayPtr);
      size = heapObjSize(array) / bytesPerBucket;
      h = (*doHash)(keyCell, size);
    }

    // remove the next node from the free list
    int64_t free = cellInt(array->free);
    array->free = array->buckets[free].next;

    // initialize the new node
    array->buckets[free].key = keyCell;
    array->buckets[free].val = valueCell;
    array->buckets[free].next = array->buckets[h].first;
    array->buckets[h].first = cellMakeInt(free);

    heapObjSetSize(m, length + 1);
  }

  engine.push(cellMakeInt(0));
}

// set(m: Map[String:$T], key: String, value: $T)
static NativeFuncDefn(runtime_set_MS3) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 3 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &mCell = engine.arg(0);
  Cell &keyCell = engine.arg(1);
  Cell &valueCell = engine.arg(2);
  BytecodeEngine::failOnNilPtr(keyCell);
  doSet(mCell, keyCell, valueCell, &doHashString, &doCompareStrings, engine);
}

// set(m: Map[Int:$T], key: Int, value: $T)
static NativeFuncDefn(runtime_set_MI3) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 3 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &mCell = engine.arg(0);
  Cell &keyCell = engine.arg(1);
  Cell &valueCell = engine.arg(2);
  doSet(mCell, keyCell, valueCell, &doHashInt, &doCompareInts, engine);
}

static void doDelete(Cell &mCell, Cell &keyCell,
		     int64_t (*doHash)(Cell &cell, int64_t size),
		     bool (*doCompare)(Cell &cell1, Cell &cell2),
		     BytecodeEngine &engine) {
  MapHandle *m = (MapHandle *)cellPtr(mCell);
  engine.failOnNilPtr(m);
  MapArray *array = (MapArray *)cellPtr(m->arrayPtr);
  int64_t length = heapObjSize(m);
  int64_t size = array ? heapObjSize(array) / bytesPerBucket : 0;

  if (length > 0) {
    int64_t h = (*doHash)(keyCell, size);
    int64_t prevIdx = size;
    int64_t idx = cellInt(array->buckets[h].first);
    while (idx < size) {
      if ((*doCompare)(array->buckets[idx].key, keyCell)) {
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
      array->buckets[idx].val = cellMakeNilHeapPtr();
      array->buckets[idx].next = array->free;
      array->free = cellMakeInt(idx);

      // reduce length and shrink the array if needed
      heapObjSetSize(m, length - 1);
      mapShrink(mCell, doHash, engine);
    }
  }

  engine.push(cellMakeInt(0));
}

// delete(m: Map[String:$T], key: String)
static NativeFuncDefn(runtime_delete_MS2) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &mCell = engine.arg(0);
  Cell &keyCell = engine.arg(1);
  BytecodeEngine::failOnNilPtr(keyCell);
  doDelete(mCell, keyCell, &doHashString, &doCompareStrings, engine);
}

// delete(m: Map[Int], key: Int)
static NativeFuncDefn(runtime_delete_MI2) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &mCell = engine.arg(0);
  Cell &keyCell = engine.arg(1);
  doDelete(mCell, keyCell, &doHashInt, &doCompareInts, engine);
}

// clear(m: Map[$K:$T])
static NativeFuncDefn(runtime_clear_M1) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &mCell = engine.arg(0);

  MapHandle *m = (MapHandle *)cellPtr(mCell);
  engine.failOnNilPtr(m);
  m->arrayPtr = cellMakeNilHeapPtr();
  heapObjSetSize(m, 0);

  engine.push(cellMakeInt(0));
}

// ifirst(m: Map[$K:$T]) -> Int
static NativeFuncDefn(runtime_ifirst_M1) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &mCell = engine.arg(0);

  MapHandle *m = (MapHandle *)cellPtr(mCell);
  engine.failOnNilPtr(m);
  MapArray *array = (MapArray *)cellPtr(m->arrayPtr);
  int64_t size = array ? (heapObjSize(array) / bytesPerBucket) : 0;
  int64_t iter;
  for (iter = 0; iter < size && cellIsNilHeapPtr(array->buckets[iter].key); ++iter) ;

  engine.push(cellMakeInt(iter));
}

// imore(m: Map[$K:$T], iter: Int) -> Bool
static NativeFuncDefn(runtime_imore_M2) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &mCell = engine.arg(0);
  Cell &iterCell = engine.arg(1);

  MapHandle *m = (MapHandle *)cellPtr(mCell);
  engine.failOnNilPtr(m);
  MapArray *array = (MapArray *)cellPtr(m->arrayPtr);
  int64_t size = array ? (heapObjSize(array) / bytesPerBucket) : 0;
  int64_t iter = cellInt(iterCell);

  engine.push(cellMakeBool(iter < size));
}

// inext(m: Map[$K:$T], iter: Int) -> Int
static NativeFuncDefn(runtime_inext_M2) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &mCell = engine.arg(0);
  Cell &iterCell = engine.arg(1);

  MapHandle *m = (MapHandle *)cellPtr(mCell);
  engine.failOnNilPtr(m);
  MapArray *array = (MapArray *)cellPtr(m->arrayPtr);
  int64_t size = array ? (heapObjSize(array) / bytesPerBucket) : 0;
  int64_t iter = cellInt(iterCell);
  if (iter < size) {
    for (++iter; iter < size && cellIsNilHeapPtr(array->buckets[iter].key); ++iter) ;
  }

  engine.push(cellMakeInt(iter));
}

// iget(m: Map[$K:$T], iter: Int) -> $K
static NativeFuncDefn(runtime_iget_M2) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &mCell = engine.arg(0);
  Cell &iterCell = engine.arg(1);

  MapHandle *m = (MapHandle *)cellPtr(mCell);
  engine.failOnNilPtr(m);
  MapArray *array = (MapArray *)cellPtr(m->arrayPtr);
  int64_t size = array ? (heapObjSize(array) / bytesPerBucket) : 0;
  int64_t iter = cellInt(iterCell);
  if (iter < 0 || iter >= size) {
    BytecodeEngine::fatalError("Index out of bounds");
  }

  engine.push(array->buckets[iter].key);
}

void runtime_Map_init(BytecodeEngine &engine) {
  engine.addNativeFunction("_allocMap", &runtime_allocMap);

  engine.addNativeFunction("length_MS1", &runtime_length_M1);
  engine.addNativeFunction("contains_MS2", &runtime_contains_MS2);
  engine.addNativeFunction("get_MS2", &runtime_get_MS2);
  engine.addNativeFunction("set_MS3", &runtime_set_MS3);
  engine.addNativeFunction("delete_MS2", &runtime_delete_MS2);
  engine.addNativeFunction("clear_MS1", &runtime_clear_M1);
  engine.addNativeFunction("ifirst_MS1", &runtime_ifirst_M1);
  engine.addNativeFunction("imore_MS2", &runtime_imore_M2);
  engine.addNativeFunction("inext_MS2", &runtime_inext_M2);
  engine.addNativeFunction("iget_MS2", &runtime_iget_M2);

  engine.addNativeFunction("length_MI1", &runtime_length_M1);
  engine.addNativeFunction("contains_MI2", &runtime_contains_MI2);
  engine.addNativeFunction("get_MI2", &runtime_get_MI2);
  engine.addNativeFunction("set_MI3", &runtime_set_MI3);
  engine.addNativeFunction("delete_MI2", &runtime_delete_MI2);
  engine.addNativeFunction("clear_MI1", &runtime_clear_M1);
  engine.addNativeFunction("ifirst_MI1", &runtime_ifirst_M1);
  engine.addNativeFunction("imore_MI2", &runtime_imore_M2);
  engine.addNativeFunction("inext_MI2", &runtime_inext_M2);
  engine.addNativeFunction("iget_MI2", &runtime_iget_M2);
}
