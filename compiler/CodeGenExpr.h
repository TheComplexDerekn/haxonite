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

// Result of generating code for an expression.
//
// On error: ok=false; type is invalid.
// On success, with no value: ok=true, type=null.
// On success, with value: ok=true, type = value type.
struct ExprResult {
  ExprResult(): ok(false) {}
  explicit ExprResult(std::unique_ptr<CTypeRef> aType): ok(true), type(std::move(aType)) {}

  bool ok;
  std::unique_ptr<CTypeRef> type;
};

//------------------------------------------------------------------------

// Generate bytecode for [expr] and add it to [bcFunc].
extern ExprResult codeGenExpr(Expr *expr, Context &ctx, BytecodeFile &bcFunc);

// Find the function matching [name] and [argResults]. Returns the
// CFuncDecl, or null if no matching function is found.
extern CFuncDecl *findFunction(const std::string &name, std::vector<ExprResult> &argResults,
			       Context &ctx);

// Find the field [fieldName] of type [objType], or null if not found.
CField *findField(CTypeRef *objType, const std::string &fieldName, Location &loc, const char *use);

#endif // CodeGenExpr_h
