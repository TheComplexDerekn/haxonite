//========================================================================
//
// runtime_Vector.h
//
// Runtime library: Vector functions.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef runtime_Vector_h
#define runtime_Vector_h

#include "BytecodeEngine.h"

// Construct an empty vector on the heap.
// NB: the caller is responsible for making the returned Cell visible
// to the GC.
// NB: this may trigger GC.
extern Cell vectorMake(BytecodeEngine &engine);

// Return the length of [vCell].
extern int64_t vectorLength(Cell &vCell);

// Return the [idx]th element of [vCell]. This does bounds checking,
// and throws a fatal error if [idx] is out of bounds.
extern Cell vectorGet(Cell &vCell, int64_t idx);

// Append [elemCell] to [vCell].
// NB: this may trigger GC.
extern void vectorAppend(Cell &vCell, Cell &elemCell, BytecodeEngine &engine);

extern void runtime_Vector_init(BytecodeEngine &engine);

#endif // runtime_Vector_h
