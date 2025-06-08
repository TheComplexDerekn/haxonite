//========================================================================
//
// FunctionChecker.h
//
// Check the function decls/defns.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef FunctionChecker_h
#define FunctionChecker_h

#include "Context.h"

//------------------------------------------------------------------------

// Check the functions in [ctx]:
// - there cannot be duplicate functions
// - there must be a main() function
// Returns true if there are no errors.
extern bool checkFunctions(Context &ctx);

#endif // FunctionChecker_h
