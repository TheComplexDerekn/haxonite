//========================================================================
//
// Link.h
//
// Link modules into a single executable file.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef Link_h
#define Link_h

#include "Context.h"

//------------------------------------------------------------------------

// Link the bytecode for all modules into an executable bytecode file.
extern bool linkExecutable(Context &ctx);

#endif // Link_h
