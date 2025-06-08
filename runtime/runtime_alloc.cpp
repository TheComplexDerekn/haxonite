//========================================================================
//
// runtime_alloc.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "runtime_alloc.h"

static NativeFuncDefn(runtime_allocStruct) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsInt(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &arg = engine.arg(0);
  int64_t size = cellInt(arg);

  uint64_t *ptr = (uint64_t *)engine.heapAllocTuple((uint64_t)size, 0);
  for (int64_t i = 0; i < size; ++i) {
    ptr[1 + i] = cellMakeInt(0);
  }

  engine.push(cellMakeHeapPtr(ptr));
}

static NativeFuncDefn(runtime_allocFuncPtr) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      (!cellIsBytecodeAddr(engine.arg(0)) && !cellIsNativePtr(engine.arg(0)))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &arg = engine.arg(0);

  uint64_t *ptr = (uint64_t *)engine.heapAllocTuple(1, 0);
  ptr[1] = arg;

  engine.push(cellMakeHeapPtr(ptr));
}

// _allocFuncPtrApply(func: func ptr, arg: any)
static NativeFuncDefn(runtime_allocFuncPtrApply) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsHeapPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &funcPtrCell = engine.arg(0);
  Cell &arg = engine.arg(1);
  Cell *funcPtr = (Cell *)cellHeapPtr(funcPtrCell);
  engine.failOnNilPtr(funcPtr);
  if (heapObjGCTag(funcPtr) != gcTagTuple) {
    BytecodeEngine::fatalError("Invalid argument");
  }
  int64_t funcPtrSize = heapObjSize(funcPtr) / 8;

  // NB: this may trigger GC
  Cell *newFuncPtr = (Cell *)engine.heapAllocTuple(funcPtrSize + 1, 0);

  funcPtr = (Cell *)cellHeapPtr(funcPtrCell);
  for (int64_t i = 0; i < funcPtrSize; ++i) {
    newFuncPtr[1+i] = funcPtr[1+i];
  }
  newFuncPtr[1+funcPtrSize] = arg;

  engine.push(cellMakeHeapPtr(newFuncPtr));
}

void runtime_alloc_init(BytecodeEngine &engine) {
  engine.addNativeFunction("_allocStruct", &runtime_allocStruct);
  engine.addNativeFunction("_allocFuncPtr", &runtime_allocFuncPtr);
  engine.addNativeFunction("_allocFuncPtrApply", &runtime_allocFuncPtrApply);
}
