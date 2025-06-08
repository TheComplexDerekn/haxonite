//========================================================================
//
// Mangle.h
//
// Function name mangler.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef Mangle_h
#define Mangle_h

#include <string>
#include "CTree.h"

//------------------------------------------------------------------------

extern std::string mangleFunctionName(CFuncDecl *func);

#endif // Mangle_h
