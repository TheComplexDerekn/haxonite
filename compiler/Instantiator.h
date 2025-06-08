//========================================================================
//
// Instantiator.h
//
// Instantiate a container type module. Creates types and functions
// with concrete types and adds them to the Context.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef Instantiator_h
#define Instantiator_h

#include "AST.h"
#include "Context.h"
#include "CTree.h"

//------------------------------------------------------------------------

// Instantiate all container types referenced in structs and function
// declarations in [ctx]. Returns true on success.
extern bool instantiateContainerTypes(Context &ctx);

// If [typeRef] refers to a container type:
// * Check that the type is valid (only certain types are allowed as
//   Set/Map keys).
// * Instantiate the type, if it hasn't been instantiated yet.
// * Return true on success.
// If [typeRef] is not a parameterized type: return true.
extern bool instantiateTypeRef(CTypeRef *typeRef, Context &ctx);

#endif // Instantiator_h
