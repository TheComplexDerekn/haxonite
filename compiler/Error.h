//========================================================================
//
// Error.h
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef Error_h
#define Error_h

#include "Location.h"

extern void error(Location loc, const char *fmt, ...);

extern void bytecodeError(const std::string &msg);

#endif // Error_h
