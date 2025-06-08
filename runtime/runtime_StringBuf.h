//========================================================================
//
// runtime_StringBuf.h
//
// Runtime library: StringBuf functions.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef runtime_StringBuf_h
#define runtime_StringBuf_h

#include "BytecodeEngine.h"

// Return the length of [sbCell].
extern int64_t stringBufLength(Cell &sbCell);

// Return the data pointer of [sbCell]. The returned pointer is
// invalidated by anything that can trigger GC.
extern uint8_t *stringBufData(Cell &sbCell);

// Append [n] bytes at [buf] to [sbCell].
// NB: this may trigger GC.
extern void stringBufAppend(Cell &sbCell, uint8_t *buf, int64_t n, BytecodeEngine &engine);

// Append [s] to [sbCell].
// NB: this may trigger GC.
extern void stringBufAppendString(Cell &sbCell, Cell &sCell, BytecodeEngine &engine);

extern void runtime_StringBuf_init(BytecodeEngine &engine);

#endif // runtime_StringBuf_h
