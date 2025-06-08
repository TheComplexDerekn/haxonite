//========================================================================
//
// DateTime.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "DateTime.h"

//------------------------------------------------------------------------

int DateTime::cmp(DateTime other) {
  if (mT < other.mT) {
    return -1;
  }
  if (mT > other.mT) {
    return 1;
  }
  if (mNS < other.mNS) {
    return -1;
  }
  if (mNS > other.mNS) {
    return 1;
  }
  return 0;
}
