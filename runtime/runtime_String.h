//========================================================================
//
// runtime_String.h
//
// Runtime library: String functions.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef runtime_String_h
#define runtime_String_h

#include "BytecodeEngine.h"

// Return the length of [s].
extern int64_t stringLength(Cell &s);

// Return the data pointer of [s]. If the string is on the heap, the
// returned pointer is invalidated by anything that can trigger GC.
extern uint8_t *stringData(Cell &s);

// Convert [s] to a std::string.
extern std::string stringToStdString(Cell &s);

// Construct an uninitialized string of [length] bytes on the heap.
// Returns a pointer to the string object.
// NB: the caller is responsible for making the returned Cell visible
// to the GC.
// NB: this may trigger GC.
extern Cell stringAlloc(int64_t length, BytecodeEngine &engine);

// Construct a string on the heap from [length] bytes at
// [data]. Returns a pointer to the string object.
// NB: the caller is responsible for making the returned Cell visible
// to the GC.
// NB: this may trigger GC.
extern Cell stringMake(const uint8_t *data, int64_t length, BytecodeEngine &engine);

// Construct a string on the heap from bytes [offset] .. [offset] +
// [length] - 1 of string [s]. Returns a pointer to the string object.
// NB: the caller is responsible for making the returned Cell visible
// to the GC.
// NB: this may trigger GC.
extern Cell stringMake(Cell &s, int64_t offset, int64_t length, BytecodeEngine &engine);

// Compare two strings and return +1/0/-1.
extern int64_t stringCompare(Cell &s1, Cell &s2);

// Concatenate [s1] and [s2], allocating a new string on the
// heap. Returns a pointer to the string object, i.e., the 8-byte
// header.
// NB: this may trigger GC.
extern uint8_t *stringConcat(Cell &s1, Cell &s2, BytecodeEngine &engine);

extern void runtime_String_init(BytecodeEngine &engine);

#endif // runtime_String_h
