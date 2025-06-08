//========================================================================
//
// runtime_datetime.h
//
// Runtime library: date/time functions.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef runtime_datetime_h
#define runtime_datetime_h

#include "BytecodeEngine.h"

extern void runtime_datetime_init(BytecodeEngine &engine);

// Create a Timestamp object.
// NB: this may trigger GC.
extern Cell timestampMake(int64_t seconds, int64_t nanoseconds, BytecodeEngine &engine);

#endif // runtime_datetime_h
