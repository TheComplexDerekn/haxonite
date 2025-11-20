//========================================================================
//
// CodeGenExpr.h
//
// Generate bytecode for expressions.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef CodeGenExpr_h
#define CodeGenExpr_h

#include <memory>
#include "AST.h"
#include "BytecodeFile.h"
#include "Context.h"
#include "CTree.h"

//------------------------------------------------------------------------

// Generate bytecode for [expr] and add it to [bcFunc].
extern ExprResult codeGenExpr(Expr *expr, Context &ctx, BytecodeFile &bcFunc);

// Find the field [fieldName] of type [objType], or null if not found.
CField *findField(CTypeRef *objType, const std::string &fieldName, Location &loc, const char *use);

#endif // CodeGenExpr_h
