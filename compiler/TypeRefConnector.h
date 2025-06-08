//========================================================================
//
// TypeRefConnector.h
//
// Connect CTypeRefs to their CTypes.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef TypeRefConnector_h
#define TypeRefConnector_h

#include <memory>
#include "AST.h"
#include "Context.h"
#include "CTree.h"

//------------------------------------------------------------------------

// Scan all CTypeRefs in [ctx] and connect them to their CTypes, i.e.,
// set CTypeRef.type and clear CTypeRef.name. Returns true on success,
// or false if any errors were detected.
extern bool connectTypeRefs(Context &ctx);

// Convert a TypeRef to a CTypeRef. Returns null on error.
extern std::unique_ptr<CTypeRef> convertTypeRef(TypeRef *typeRef, Context &ctx);

#endif // TypeRefConnector_h
