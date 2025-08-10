//========================================================================
//
// Hash.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "Hash.h"
#include "runtime_String.h"

// This is the 64-bit FNV-1a hash.
static uint64_t hash(uint8_t *data, int64_t nBytes) {
  uint64_t h = UINT64_C(14695981039346656037);
  for (int64_t i = 0; i < nBytes; ++i) {
    h ^= data[i];
    h *= UINT64_C(1099511628211);
  }
  return h;
}

uint64_t hashInt(int64_t x) {
  return hash((uint8_t *)&x, 7);
}

uint64_t hashString(Cell &s) {
  return hash((uint8_t *)stringData(s), stringByteLength(s));
}

int64_t hashFold(uint64_t h, int64_t size) {
  int log2 = 63 - __builtin_clzl(size);
  uint64_t out = h ^ (h >> log2);
  out = out & (size - 1);
  return (int64_t)out;
}
