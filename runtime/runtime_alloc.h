//========================================================================
//
// runtime_alloc.h
//
// Low-level allocation functions visible to the compiler, but not to
// Haxonite code.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef runtime_alloc_h
#define runtime_alloc_h

#include "BytecodeEngine.h"

extern void runtime_alloc_init(BytecodeEngine &engine);

#endif // runtime_alloc_h
