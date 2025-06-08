//========================================================================
//
// ConstExpr.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "ConstExpr.h"
#include "BytecodeDefs.h"
#include "Error.h"
#include "NumConversion.h"

//------------------------------------------------------------------------

static std::unique_ptr<CConstValue> evalBinaryOpExpr(BinaryOpExpr *expr, Context &ctx);
static std::unique_ptr<CConstValue> evalUnaryOpExpr(UnaryOpExpr *expr, Context &ctx);
static std::unique_ptr<CConstValue> evalParenExpr(ParenExpr *expr, Context &ctx);
static std::unique_ptr<CConstValue> evalIdentExpr(IdentExpr *expr, Context &ctx);
static std::unique_ptr<CConstValue> evalLitIntExpr(LitIntExpr *expr, Context &ctx);
static std::unique_ptr<CConstValue> evalLitFloatExpr(LitFloatExpr *expr, Context &ctx);
static std::unique_ptr<CConstValue> evalLitBoolExpr(LitBoolExpr *expr, Context &ctx);
static std::unique_ptr<CConstValue> evalLitStringExpr(LitStringExpr *expr, Context &ctx);

//------------------------------------------------------------------------

std::unique_ptr<CConstValue> evalConstExpr(Expr *expr, Context &ctx) {
  switch (expr->kind()) {
  case Expr::Kind::binaryOpExpr:
    return evalBinaryOpExpr((BinaryOpExpr *)expr, ctx);
  case Expr::Kind::unaryOpExpr:
    return evalUnaryOpExpr((UnaryOpExpr *)expr, ctx);
  case Expr::Kind::parenExpr:
    return evalParenExpr((ParenExpr *)expr, ctx);
  case Expr::Kind::identExpr:
    return evalIdentExpr((IdentExpr *)expr, ctx);
  case Expr::Kind::litIntExpr:
    return evalLitIntExpr((LitIntExpr *)expr, ctx);
  case Expr::Kind::litFloatExpr:
    return evalLitFloatExpr((LitFloatExpr *)expr, ctx);
  case Expr::Kind::litBoolExpr:
    return evalLitBoolExpr((LitBoolExpr *)expr, ctx);
  case Expr::Kind::litStringExpr:
    return evalLitStringExpr((LitStringExpr *)expr, ctx);
  default:
    error(expr->loc, "Invalid constant expression");
    return nullptr;
  }
}

static std::unique_ptr<CConstValue> evalBinaryOpExpr(BinaryOpExpr *expr, Context &ctx) {
  std::unique_ptr<CConstValue> lhs = evalConstExpr(expr->lhs.get(), ctx);
  std::unique_ptr<CConstValue> rhs = evalConstExpr(expr->rhs.get(), ctx);
  if (!lhs || !rhs) {
    return nullptr;
  }
  CConstValue *lhsPtr = lhs.get();
  CConstValue *rhsPtr = rhs.get();

  switch (expr->op) {
  case BinaryOp::orOp:
    if (lhs->isInt() && rhs->isInt()) {
      return std::make_unique<CConstIntValue>(((CConstIntValue *)lhsPtr)->val |
					      ((CConstIntValue *)rhsPtr)->val);
    } else if (lhs->isBool() && rhs->isBool()) {
      return std::make_unique<CConstBoolValue>(((CConstBoolValue *)lhsPtr)->val |
					       ((CConstBoolValue *)rhsPtr)->val);
    } else {
      error(expr->loc, "Invalid types for '|' operator in constant expression");
      return nullptr;
    }
  case BinaryOp::xorOp:
    if (lhs->isInt() && rhs->isInt()) {
      return std::make_unique<CConstIntValue>(((CConstIntValue *)lhsPtr)->val ^
					      ((CConstIntValue *)rhsPtr)->val);
    } else if (lhs->isBool() && rhs->isBool()) {
      return std::make_unique<CConstBoolValue>(((CConstBoolValue *)lhsPtr)->val ^
					       ((CConstBoolValue *)rhsPtr)->val);
    } else {
      error(expr->loc, "Invalid types for '^' operator in constant expression");
      return nullptr;
    }
  case BinaryOp::andOp:
    if (lhs->isInt() && rhs->isInt()) {
      return std::make_unique<CConstIntValue>(((CConstIntValue *)lhsPtr)->val &
					      ((CConstIntValue *)rhsPtr)->val);
    } else if (lhs->isBool() && rhs->isBool()) {
      return std::make_unique<CConstBoolValue>(((CConstBoolValue *)lhsPtr)->val &
					       ((CConstBoolValue *)rhsPtr)->val);
    } else {
      error(expr->loc, "Invalid types for '&' operator in constant expression");
      return nullptr;
    }
  case BinaryOp::eq:
    if (lhs->isInt() && rhs->isInt()) {
      return std::make_unique<CConstBoolValue>(((CConstIntValue *)lhsPtr)->val ==
					       ((CConstIntValue *)rhsPtr)->val);
    } else if (lhs->isFloat() && rhs->isFloat()) {
      return std::make_unique<CConstBoolValue>(((CConstFloatValue *)lhsPtr)->val ==
					       ((CConstFloatValue *)rhsPtr)->val);
    } else if (lhs->isBool() && rhs->isBool()) {
      return std::make_unique<CConstBoolValue>(((CConstBoolValue *)lhsPtr)->val ==
					       ((CConstBoolValue *)rhsPtr)->val);
    } else if (lhs->isString() && rhs->isString()) {
      return std::make_unique<CConstBoolValue>(((CConstStringValue *)lhsPtr)->val ==
					       ((CConstStringValue *)rhsPtr)->val);
    } else {
      error(expr->loc, "Invalid types for '==' operator in constant expression");
      return nullptr;
    }
  case BinaryOp::ne:
    if (lhs->isInt() && rhs->isInt()) {
      return std::make_unique<CConstBoolValue>(((CConstIntValue *)lhsPtr)->val !=
					       ((CConstIntValue *)rhsPtr)->val);
    } else if (lhs->isFloat() && rhs->isFloat()) {
      return std::make_unique<CConstBoolValue>(((CConstFloatValue *)lhsPtr)->val !=
					       ((CConstFloatValue *)rhsPtr)->val);
    } else if (lhs->isBool() && rhs->isBool()) {
      return std::make_unique<CConstBoolValue>(((CConstBoolValue *)lhsPtr)->val !=
					       ((CConstBoolValue *)rhsPtr)->val);
    } else if (lhs->isString() && rhs->isString()) {
      return std::make_unique<CConstBoolValue>(((CConstStringValue *)lhsPtr)->val !=
					       ((CConstStringValue *)rhsPtr)->val);
    } else {
      error(expr->loc, "Invalid types for '!=' operator in constant expression");
      return nullptr;
    }
  case BinaryOp::same:
    error(expr->loc, "The '===' operator is not allowed in constant expressions");
    return nullptr;
  case BinaryOp::notSame:
    error(expr->loc, "The '!==' operator is not allowed in constant expressions");
    return nullptr;
  case BinaryOp::lt:
    if (lhs->isInt() && rhs->isInt()) {
      return std::make_unique<CConstBoolValue>(((CConstIntValue *)lhsPtr)->val <
					       ((CConstIntValue *)rhsPtr)->val);
    } else if (lhs->isFloat() && rhs->isFloat()) {
      return std::make_unique<CConstBoolValue>(((CConstFloatValue *)lhsPtr)->val <
					       ((CConstFloatValue *)rhsPtr)->val);
    } else if (lhs->isString() && rhs->isString()) {
      return std::make_unique<CConstBoolValue>(((CConstStringValue *)lhsPtr)->val <
					       ((CConstStringValue *)rhsPtr)->val);
    } else {
      error(expr->loc, "Invalid types for '<' operator in constant expression");
      return nullptr;
    }
  case BinaryOp::gt:
    if (lhs->isInt() && rhs->isInt()) {
      return std::make_unique<CConstBoolValue>(((CConstIntValue *)lhsPtr)->val >
					       ((CConstIntValue *)rhsPtr)->val);
    } else if (lhs->isFloat() && rhs->isFloat()) {
      return std::make_unique<CConstBoolValue>(((CConstFloatValue *)lhsPtr)->val >
					       ((CConstFloatValue *)rhsPtr)->val);
    } else if (lhs->isString() && rhs->isString()) {
      return std::make_unique<CConstBoolValue>(((CConstStringValue *)lhsPtr)->val >
					       ((CConstStringValue *)rhsPtr)->val);
    } else {
      error(expr->loc, "Invalid types for '>' operator in constant expression");
      return nullptr;
    }
  case BinaryOp::le:
    if (lhs->isInt() && rhs->isInt()) {
      return std::make_unique<CConstBoolValue>(((CConstIntValue *)lhsPtr)->val <=
					       ((CConstIntValue *)rhsPtr)->val);
    } else if (lhs->isFloat() && rhs->isFloat()) {
      return std::make_unique<CConstBoolValue>(((CConstFloatValue *)lhsPtr)->val <=
					       ((CConstFloatValue *)rhsPtr)->val);
    } else if (lhs->isString() && rhs->isString()) {
      return std::make_unique<CConstBoolValue>(((CConstStringValue *)lhsPtr)->val <=
					       ((CConstStringValue *)rhsPtr)->val);
    } else {
      error(expr->loc, "Invalid types for '<=' operator in constant expression");
      return nullptr;
    }
  case BinaryOp::ge:
    if (lhs->isInt() && rhs->isInt()) {
      return std::make_unique<CConstBoolValue>(((CConstIntValue *)lhsPtr)->val >=
					       ((CConstIntValue *)rhsPtr)->val);
    } else if (lhs->isFloat() && rhs->isFloat()) {
      return std::make_unique<CConstBoolValue>(((CConstFloatValue *)lhsPtr)->val >=
					       ((CConstFloatValue *)rhsPtr)->val);
    } else if (lhs->isString() && rhs->isString()) {
      return std::make_unique<CConstBoolValue>(((CConstStringValue *)lhsPtr)->val >=
					       ((CConstStringValue *)rhsPtr)->val);
    } else {
      error(expr->loc, "Invalid types for '>=' operator in constant expression");
      return nullptr;
    }
  case BinaryOp::shl:
    if (lhs->isInt() && rhs->isInt()) {
      return std::make_unique<CConstIntValue>(((CConstIntValue *)lhsPtr)->val <<
					      ((CConstIntValue *)rhsPtr)->val);
    } else {
      error(expr->loc, "Invalid types for '<<' operator in constant expression");
      return nullptr;
    }
  case BinaryOp::shr:
    if (lhs->isInt() && rhs->isInt()) {
      return std::make_unique<CConstIntValue>(((CConstIntValue *)lhsPtr)->val >>
					      ((CConstIntValue *)rhsPtr)->val);
    } else {
      error(expr->loc, "Invalid types for '>>' operator in constant expression");
      return nullptr;
    }
  case BinaryOp::add:
    if (lhs->isInt() && rhs->isInt()) {
      int64_t x = ((CConstIntValue *)lhsPtr)->val + ((CConstIntValue *)rhsPtr)->val;
      if (x > bytecodeMaxInt || x < bytecodeMinInt) {
	error(expr->loc, "Integer overflow in constant expression");
	return nullptr;
      }
      return std::make_unique<CConstIntValue>(x);
    } else if (lhs->isFloat() && rhs->isFloat()) {
      return std::make_unique<CConstFloatValue>(((CConstFloatValue *)lhsPtr)->val +
						((CConstFloatValue *)rhsPtr)->val);
    } else if (lhs->isString() && rhs->isString()) {
      return std::make_unique<CConstStringValue>(((CConstStringValue *)lhsPtr)->val +
						 ((CConstStringValue *)rhsPtr)->val);
    } else {
      error(expr->loc, "Invalid types for '+' operator in constant expression");
      return nullptr;
    }
  case BinaryOp::sub:
    if (lhs->isInt() && rhs->isInt()) {
      int64_t x = ((CConstIntValue *)lhsPtr)->val - ((CConstIntValue *)rhsPtr)->val;
      if (x > bytecodeMaxInt || x < bytecodeMinInt) {
	error(expr->loc, "Integer overflow in constant expression");
	return nullptr;
      }
      return std::make_unique<CConstIntValue>(x);
    } else if (lhs->isFloat() && rhs->isFloat()) {
      return std::make_unique<CConstFloatValue>(((CConstFloatValue *)lhsPtr)->val -
						((CConstFloatValue *)rhsPtr)->val);
    } else {
      error(expr->loc, "Invalid types for '-' operator in constant expression");
      return nullptr;
    }
  case BinaryOp::mul:
    if (lhs->isInt() && rhs->isInt()) {
      int64_t x;
      if (__builtin_mul_overflow(((CConstIntValue *)lhsPtr)->val,
				 ((CConstIntValue *)rhsPtr)->val,
				 &x) ||
	  x > bytecodeMaxInt || x < bytecodeMinInt) {
	error(expr->loc, "Integer overflow in constant expression");
	return nullptr;
      }
      return std::make_unique<CConstIntValue>(x);
    } else if (lhs->isFloat() && rhs->isFloat()) {
      return std::make_unique<CConstFloatValue>(((CConstFloatValue *)lhsPtr)->val *
						((CConstFloatValue *)rhsPtr)->val);
    } else {
      error(expr->loc, "Invalid types for '*' operator in constant expression");
      return nullptr;
    }
  case BinaryOp::div:
    if (lhs->isInt() && rhs->isInt()) {
      int64_t rhsVal = ((CConstIntValue *)rhsPtr)->val;
      if (rhsVal == 0) {
	error(expr->loc, "Integer divide-by-zero in constant expression");
	return nullptr;
      }
      int64_t x = ((CConstIntValue *)lhsPtr)->val / rhsVal;
      // overflow case: minInt / -1 --> maxInt + 1
      if (x > bytecodeMaxInt) {
	error(expr->loc, "Integer overflow in constant expression");
	return nullptr;
      }
      return std::make_unique<CConstIntValue>(x);
    } else if (lhs->isFloat() && rhs->isFloat()) {
      return std::make_unique<CConstFloatValue>(((CConstFloatValue *)lhsPtr)->val /
						((CConstFloatValue *)rhsPtr)->val);
    } else {
      error(expr->loc, "Invalid types for '/' operator in constant expression");
      return nullptr;
    }
  case BinaryOp::mod:
    if (lhs->isInt() && rhs->isInt()) {
      int64_t rhsVal = ((CConstIntValue *)rhsPtr)->val;
      if (rhsVal == 0) {
	error(expr->loc, "Integer divide-by-zero in constant expression");
	return nullptr;
      }
      int64_t x = ((CConstIntValue *)lhsPtr)->val % rhsVal;
      return std::make_unique<CConstIntValue>(x);
    } else {
      error(expr->loc, "Invalid types for '%' operator in constant expression");
      return nullptr;
    }
  default:
    error(expr->loc, "Internal (evalBinaryOpExpr)");
    return nullptr;
  }
}

static std::unique_ptr<CConstValue> evalUnaryOpExpr(UnaryOpExpr *expr, Context &ctx) {
  std::unique_ptr<CConstValue> val = evalConstExpr(expr->expr.get(), ctx);
  if (!val) {
    return nullptr;
  }
  CConstValue *valPtr = val.get();

  switch (expr->op) {
  case UnaryOp::neg:
    if (val->isInt()) {
      int64_t x = -((CConstIntValue *)valPtr)->val;
      // overflow case: -minInt --> maxInt + 1
      if (x > bytecodeMaxInt) {
	error(expr->loc, "Integer overflow in constant expression");
	return nullptr;
      }
      return std::make_unique<CConstIntValue>(x);
    } else if (val->isFloat()) {
      return std::make_unique<CConstFloatValue>(-((CConstFloatValue *)valPtr)->val);
    } else {
      error(expr->loc, "Invalid type for unary '-' operator in constant expression");
      return nullptr;
    }
  case UnaryOp::notOp:
    if (val->isInt()) {
      return std::make_unique<CConstIntValue>(~((CConstIntValue *)valPtr)->val);
    } else if (val->isBool()) {
      return std::make_unique<CConstBoolValue>(!((CConstBoolValue *)valPtr)->val);
    } else {
      error(expr->loc, "Invalid type for '!' operator in constant expression");
      return nullptr;
    }
  case UnaryOp::length:
    if (val->isString()) {
      return std::make_unique<CConstIntValue>(
		      (int64_t)((CConstStringValue *)valPtr)->val.length());
    } else {
      error(expr->loc, "Invalid type for '#' operator in constant expression");
      return nullptr;
    }
  default:
    error(expr->loc, "Internal (evalUnaryOpExpr)");
    return nullptr;
  }
}

static std::unique_ptr<CConstValue> evalParenExpr(ParenExpr *expr, Context &ctx) {
  return evalConstExpr(expr->expr.get(), ctx);
}

static std::unique_ptr<CConstValue> evalIdentExpr(IdentExpr *expr, Context &ctx) {
  CSymbol *sym = ctx.findSymbol(expr->name);
  if (!sym) {
    error(expr->loc, "Symbol '%s' is undefined", expr->name.c_str());
    return nullptr;
  }
  if (sym->kind() != CSymbolKind::constant) {
    error(expr->loc, "Symbol '%s' is not a constant", expr->name.c_str());
    return nullptr;
  }
  return ((CConst *)sym)->value->copy();
}

static std::unique_ptr<CConstValue> evalLitIntExpr(LitIntExpr *expr, Context &ctx) {
  int64_t val;
  if (!stringToInt56(expr->val, expr->radix, val)) {
    error(expr->loc, "Integer literal out of bounds");
    return nullptr;
  }
  return std::make_unique<CConstIntValue>(val);
}

static std::unique_ptr<CConstValue> evalLitFloatExpr(LitFloatExpr *expr, Context &ctx) {
  float val;
  stringToFloat(expr->val, val);
  return std::make_unique<CConstFloatValue>(val);
}

static std::unique_ptr<CConstValue> evalLitBoolExpr(LitBoolExpr *expr, Context &ctx) {
  return std::make_unique<CConstBoolValue>(expr->val);
}

static std::unique_ptr<CConstValue> evalLitStringExpr(LitStringExpr *expr, Context &ctx) {
  return std::make_unique<CConstStringValue>(expr->val);
}
