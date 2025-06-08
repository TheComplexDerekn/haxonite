//========================================================================
//
// CodeGenFunc.h
//
// Generate bytecode for functions.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef CodeGenFunc_h
#define CodeGenFunc_h

#include "AST.h"
#include "BytecodeFile.h"
#include "Context.h"

//------------------------------------------------------------------------

// Generate bytecode for [func] and add it to [writer].  Returns true
// on success.
extern bool codeGenFunc(FuncDefn *func, Context &ctx, BytecodeFile &bcFile);

#endif // CodeGenFunc_h
