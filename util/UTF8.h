//========================================================================
//
// UTF8.h
//
// Functions to process UTF-8 strings.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef UTF8_h
#define UTF8_h

#include <stdint.h>
#include <string>

//------------------------------------------------------------------------

// Max length of a UTF-8 sequence (as supported by this module).
#define utf8MaxBytes 6

//------------------------------------------------------------------------

// Parse one UTF-8 codepoint from [s] (of length [length]), starting
// at [idx].
//
// If [idx] is at (or past) the end of [s]: returns false.
//
// If there is a valid UTF-8 codepoint starting at [idx]: stores the
// codepoint in [u], updates [idx] to point to the following byte, and
// returns true.
//
// Otherwise: stores one byte in [u], increments [idx], and returns
// true.
extern bool utf8Get(const uint8_t *s, int64_t length, int64_t &idx, uint32_t &u);

// Returns the length of the UTF-8 codepoint starting at [idx] in [s].
// This follows the same rules as utf8Get: returns 1 for an invalid
// UTF-8 codepoint. Returns 0 if [idx] is at (or past) the end of [s].
extern int utf8Length(const uint8_t *s, int64_t length, int64_t idx);

// Returns the length of the UTF-8 codepoint ending at [idx]-1 in [s].
// This follows the same rules as utf8Get: returns 1 for an invalid
// UTF-8 codepoint. Returns 0 if [idx] is at the start of [s].
extern int utf8PrevLength(const uint8_t *s, int64_t length, int64_t idx);

// Encode [u] into [out], which must have room for at least
// utf8MaxBytes bytes. Returns the number of bytes in the UTF-8
// sequence (which will be in 0.. utf8MaxBytes). Returns 0 if [u] is
// not a valid Unicode codepoint (i.e., if [i] is too large).
extern int utf8Encode(uint32_t u, uint8_t *out);

#endif // UTF8_h
