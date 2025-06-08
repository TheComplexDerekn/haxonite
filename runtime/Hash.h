//========================================================================
//
// Hash.h
//
// Hash functions for types that can be used as Set/Map keys.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef Hash_h
#define Hash_h

#include "BytecodeEngine.h"

// Hash an integer.
extern uint64_t hashInt(int64_t x);

// Hash a string.
extern uint64_t hashString(Cell &s);

// Xor-fold a hash value [x], and return a value in [0, size).
// [size] must be a power of 2.
extern int64_t hashFold(uint64_t h, int64_t size);

#endif // Hash_h
