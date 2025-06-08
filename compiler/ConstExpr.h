//========================================================================
//
// ConstExpr.h
//
// Constant expression evaluator.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef ConstExpr_h
#define ConstExpr_h

#include <memory>
#include "AST.h"
#include "Context.h"
#include "CTree.h"

//------------------------------------------------------------------------

extern std::unique_ptr<CConstValue> evalConstExpr(Expr *expr, Context &ctx);

#endif // ConstExpr_h
