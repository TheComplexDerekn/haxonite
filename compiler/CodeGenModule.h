//========================================================================
//
// CodeGenModule.h
//
// Generate bytecode for modules.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef CodeGenModule_h
#define CodeGenModule_h

#include "Context.h"
#include "CTree.h"

//------------------------------------------------------------------------

extern bool codeGenModule(CModule *cmod, Context &ctx);

#endif // CodeGenModule_h
