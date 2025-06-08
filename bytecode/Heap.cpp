//========================================================================
//
// Heap.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "BytecodeEngine.h"
#include <string.h>

//------------------------------------------------------------------------

void BytecodeEngine::heapInit() {
  heapSize = initialHeapSize / 8;
  try {
    heap = std::unique_ptr<uint64_t[]>(new uint64_t[heapSize]);
  } catch (std::bad_alloc) {
    fatalError("Out of memory");
  }
  heapNext = 0;
  prevCompactedHeapSize = 0;
  resObjs = nullptr;
}

void *BytecodeEngine::heapAllocBlob(uint64_t size, uint8_t typeTag) {
  return heapAlloc(1 + (size + 7) / 8, size, typeTag, gcTagBlob);
}

void *BytecodeEngine::heapAllocTuple(uint64_t size, uint8_t typeTag) {
  return heapAlloc(1 + size, size * 8, typeTag, gcTagTuple);
}

void *BytecodeEngine::heapAllocHandle(uint64_t size, uint8_t typeTag) {
  return heapAlloc(2, size, typeTag, gcTagHandle);
}

// Allocate [nWords] 64-bit words. [nWords] must be at least one, to
// allow for the header word. The header word is filled in with
// [size], [gcTag], and [typeTag].
void *BytecodeEngine::heapAlloc(uint64_t nWords, uint64_t size, uint8_t typeTag, uint8_t gcTag) {
  if (heapSize - heapNext < nWords) {
    gc(nWords);
  }
  void *p = &heap[heapNext];
  heap[heapNext] = (size << 8) | ((typeTag & 0x3f) << 2) | (gcTag & 3);
  heapNext += nWords;
  return p;
}

void BytecodeEngine::pushGCRoot(Cell &cell) {
  gcRoots.push_back(&cell);
}

void BytecodeEngine::popGCRoot(Cell &cell) {
  if (gcRoots.empty() || gcRoots.back() != &cell) {
    fatalError("GC root stack mimatch");
  }
  gcRoots.pop_back();
}

void BytecodeEngine::addResourceObject(ResourceObject *resObj) {
  resObj->prev = nullptr;
  resObj->next = resObjs;
  if (resObjs) {
    resObjs->prev = resObj;
  }
  resObjs = resObj;
  resObj->marked = false;
}

void BytecodeEngine::removeResourceObject(ResourceObject *resObj) {
  if (resObj->next) {
    resObj->next->prev = resObj->prev;
  }
  if (resObj->prev) {
    resObj->prev->next = resObj->next;
  }
  if (resObjs == resObj) {
    resObjs = resObj->next;
  }
  resObj->prev = nullptr;
  resObj->next = nullptr;
}

size_t BytecodeEngine::currentHeapSize() {
  return heapSize * 8;
}

// Run a garbage collection. On return there will be sufficient space
// for an allocation of [nWords] 64-bit words.
void BytecodeEngine::gc(uint64_t nWords) {
  size_t newHeapSize = heapSize;
  while (newHeapSize - prevCompactedHeapSize < nWords) {
    if (newHeapSize > SIZE_MAX / 2) {
      fatalError("Out of memory");
    }
    newHeapSize *= 2;
  }

  fullGC(newHeapSize);
  scanResourceObjects();

  if (heapSize - heapNext < nWords) {
    do {
      if (newHeapSize > SIZE_MAX / 2) {
	fatalError("Out of memory");
      }
      newHeapSize *= 2;
    } while (newHeapSize - heapNext < nWords);
    quickGC(newHeapSize);
  }

  prevCompactedHeapSize = heapNext;

  if (verbose) {
    printf("** GC: compacted heap size = %zu bytes **\n", prevCompactedHeapSize * 8);
  }
}

// Allocate a new heap of [newHeapSize] words, copy over all live
// objects, and update all pointers.
void BytecodeEngine::fullGC(size_t newHeapSize) {
  if (verbose) {
    printf("** GC: new heap size = %zu bytes **\n", newHeapSize * 8);
  }

  std::unique_ptr<uint64_t[]> newHeap;
  try {
    newHeap = std::unique_ptr<uint64_t[]>(new uint64_t[newHeapSize]);
  } catch (std::bad_alloc) {
    fatalError("Out of memory");
  }
  size_t newHeapNext = 0;

  // a stack of pointer-addresses, i.e., pointers to pointers: this
  // contains addresses of Cells which contain unscanned pointers,
  // i.e., pointers to objects in the old heap
  std::vector<Cell*> ptrAddrStack;

  // this processes entries in gcRoots first, and then entries in stack
  bool processingGCRoots = true;
  size_t idx = 0;
  while (true) {

    // get the next cell from either gcRoots or stack
    Cell *cell;
    if (processingGCRoots) {
      if (idx < gcRoots.size()) {
	cell = gcRoots[idx++];
      } else {
	processingGCRoots = false;
	idx = sp;
      }
    }
    if (!processingGCRoots) {
      if (idx < stackSize) {
	cell = &stack[idx++];
      } else {
	break;
      }
    }
    if (!cellIsHeapPtr(*cell) || cellIsNilHeapPtr(*cell)) {
      continue;
    }

    // scan everything reachable from this pointer
    ptrAddrStack.push_back(cell);
    while (!ptrAddrStack.empty()) {
      Cell *ptrAddr = ptrAddrStack.back();
      ptrAddrStack.pop_back();
      uint64_t *ptr = (uint64_t *)cellHeapPtr(*ptrAddr);
      int gcTag = heapObjGCTag(ptr);
      void *newPtr;

      // already relocated - just update the pointer
      if (gcTag == gcTagRelocated) {
	newPtr = heapObjRelocatedPtr(ptr);

      // relocate and add pointers to queue
      } else {

	// compute object size
	uint64_t objSize;
	if (gcTag == gcTagHandle) {
	  objSize = 2;
	} else {
	  objSize = 1 + (heapObjSize(ptr) + 7) / 8;
	}

	// allocate space in new heap
	newPtr = &newHeap[newHeapNext];
	newHeapNext += objSize;

	// copy the object
	memcpy(newPtr, ptr, objSize * 8);

	// enqueue pointers in the relocated object
	if (gcTag != gcTagBlob) {
	  for (uint64_t i = 1; i < objSize; ++i) {
	    Cell *newPtrAddr = (Cell *)newPtr + i;
	    if (cellIsHeapPtr(*newPtrAddr) && !cellIsNilHeapPtr(*newPtrAddr)) {
	      ptrAddrStack.push_back(newPtrAddr);
	    } else if (cellIsResourcePtr(*newPtrAddr) && !cellIsNilPtr(*newPtrAddr)) {
	      ((ResourceObject *)cellResourcePtr(*newPtrAddr))->marked = true;
	    }
	  }
	}

	// mark the old object as relocated
	*ptr = (uint64_t)newPtr | gcTagRelocated;
      }

      // update the pointer
      *ptrAddr = cellMakeHeapPtr(newPtr);
    }
  }

  // zero the unallocated part of the new heap
  // (zero words are nil heap pointers)
  memset(newHeap.get() + newHeapNext, 0, (newHeapSize - newHeapNext) * 8);

  std::swap(heap, newHeap);
  heapSize = newHeapSize;
  heapNext = newHeapNext;
}

// Allocate a new heap of [newHeapSize] words, copy over all objects,
// and update all pointers. This is run immediately after a full GC,
// so the heap contains only live objects.
void BytecodeEngine::quickGC(size_t newHeapSize) {
  if (verbose) {
    printf("** GC: resize to %zu bytes **\n", newHeapSize * 8);
  }

  std::unique_ptr<uint64_t[]> newHeap;
  try {
    newHeap = std::unique_ptr<uint64_t[]>(new uint64_t[newHeapSize]);
  } catch (std::bad_alloc) {
    fatalError("Out of memory");
  }

  memcpy(newHeap.get(), heap.get(), heapNext * 8);
  size_t newHeapNext = heapNext;

  size_t delta = (char *)newHeap.get() - (char *)heap.get();

  // update pointers in gcRoots
  for (size_t idx = 0; idx < gcRoots.size(); ++idx) {
    Cell *cell = gcRoots[idx];
    if (cellIsHeapPtr(*cell) && !cellIsNilHeapPtr(*cell)) {
      *cell = cellMakeHeapPtr((char *)cellHeapPtr(*cell) + delta);
    }
  }

  // update pointers on stack
  for (size_t stackIdx = sp; stackIdx < stackSize; ++stackIdx) {
    Cell *cell = &stack[stackIdx];
    if (cellIsHeapPtr(*cell) && !cellIsNilHeapPtr(*cell)) {
      *cell = cellMakeHeapPtr((char *)cellHeapPtr(*cell) + delta);
    }
  }

  // update pointers on heap
  size_t heapIdx = 0;
  while (heapIdx < newHeapNext) {

    // compute object size
    uint64_t *ptr = &newHeap[heapIdx];
    int gcTag = heapObjGCTag(ptr);
    uint64_t objSize;
    if (gcTag == gcTagHandle) {
      objSize = 2;
    } else {
      objSize = 1 + (heapObjSize(ptr) + 7) / 8;
    }

    // update pointers
    if (gcTag != gcTagBlob) {
      for (uint64_t i = 1; i < objSize; ++i) {
	if (cellIsHeapPtr(ptr[i]) && !cellIsNilHeapPtr(ptr[i])) {
	  ptr[i] = cellMakeHeapPtr((char *)cellHeapPtr(ptr[i]) + delta);
	}
      }
    }

    heapIdx += objSize;
  }

  // zero the unallocated part of the new heap
  // (zero words are nil heap pointers)
  memset(newHeap.get() + newHeapNext, 0, (newHeapSize - newHeapNext) * 8);

  std::swap(heap, newHeap);
  heapSize = newHeapSize;
  heapNext = newHeapNext;
}

// Scan the list of resource objects. For any that are unmarked (no
// longer live), call the finalizer and remove the resource object
// from the list.
void BytecodeEngine::scanResourceObjects() {
  ResourceObject *resObj = resObjs;
  while (resObj) {
    ResourceObject *next = resObj->next;
    if (resObj->marked) {
      resObj->marked = false;
    } else {
      if (resObj->next) {
	resObj->next->prev = resObj->prev;
      }
      if (resObj->prev) {
	resObj->prev->next = resObj->next;
      }
      if (resObjs == resObj) {
	resObjs = resObj->next;
      }
      resObj->prev = nullptr;
      resObj->next = nullptr;
      (*resObj->finalizer)(resObj);
    }
    resObj = next;
  }
}
