//========================================================================
//
// CodeGenExpr.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "CodeGenExpr.h"
#include <unordered_set>
#include "BytecodeDefs.h"
#include "Error.h"
#include "Instantiator.h"
#include "Mangle.h"
#include "NumConversion.h"
#include "TypeCheck.h"
#include "TypeRefConnector.h"

//------------------------------------------------------------------------

static ExprResult codeGenBinaryOpExpr(BinaryOpExpr *expr, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenUnaryOpExpr(UnaryOpExpr *expr, Context &ctx, BytecodeFile &bcFunc);
static std::unique_ptr<CTypeRef> makeResultType(TypeCheckKind kind, Location loc, Context &ctx);
static ExprResult codeGenPropagateExpr(PropagateExpr *expr, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenCheckExpr(CheckExpr *expr, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenCallExpr(CallExpr *expr, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenFindFunc(const std::string &name, std::vector<ExprResult> &argResults,
				  Location loc, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenFindSubstructFunc(const std::string &name,
					   std::vector<ExprResult> &argResults,
					   size_t substructArgIdx,
					   Location loc, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenMemberExpr(MemberExpr *expr, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenIndexExpr(IndexExpr *expr, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenParenExpr(ParenExpr *expr, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenNewExpr(NewExpr *expr, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenNewStringBufExpr(NewExpr *expr, std::unique_ptr<CTypeRef> typeRef,
					  Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenNewContainerExpr(NewExpr *expr, std::unique_ptr<CTypeRef> typeRef,
					  CContainerType *type,
					  Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenMakeExpr(MakeExpr *expr, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenMakeStructExpr(MakeExpr *expr, std::unique_ptr<CTypeRef> typeRef,
					CStructType *type, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenMakeSubStructExpr(MakeExpr *expr, std::unique_ptr<CTypeRef> typeRef,
					   CSubStructType *type, Context &ctx,
					   BytecodeFile &bcFunc);
static ExprResult codeGenFuncPointerExpr(FuncPointerExpr *expr, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenNilExpr(NilExpr *expr, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenNilTestExpr(NilTestExpr *expr, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenErrorExpr(ErrorExpr *expr, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenValidExpr(ValidExpr *expr, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenOkExpr(OkExpr *expr, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenIdentExpr(IdentExpr *expr, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenIdent(const std::string &ident, Location loc,
			       Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenConstValue(CConstValue *val, Location loc,
				    Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenLitVectorExpr(LitVectorExpr *expr, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenLitSetExpr(LitSetExpr *expr, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenLitMapExpr(LitMapExpr *expr, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenLitIntExpr(LitIntExpr *expr, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenLitInt(const std::string &val, int radix, Location loc,
				Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenLitFloatExpr(LitFloatExpr *expr, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenLitBoolExpr(LitBoolExpr *expr, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenLitCharExpr(LitCharExpr *expr, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenLitStringExpr(LitStringExpr *expr, Context &ctx, BytecodeFile &bcFunc);
static ExprResult codeGenInterpStringExpr(InterpStringExpr *expr, Context &ctx,
					  BytecodeFile &bcFunc);
static bool codeGenInterpStringChars(InterpStringChars *chars, BytecodeFile &bcFunc);
static bool codeGenInterpStringArg(InterpStringArg *arg, Context &ctx, BytecodeFile &bcFunc);
static bool codeGenString(const std::string &s, Location loc, BytecodeFile &bcFunc);

//------------------------------------------------------------------------

struct BinaryOpInfo {
  BinaryOp op;			// operator
  TypeCheckKind in;		// both LHS and RHS must have this type
  uint8_t opcode;		// instruction to use
  TypeCheckKind out;		// result type
};

static std::vector<BinaryOpInfo> binaryOpInfo {
  { BinaryOp::orOp,    TypeCheckKind::tInt,   bcOpcodeOr,    TypeCheckKind::tInt   },
  { BinaryOp::orOp,    TypeCheckKind::tBool,  bcOpcodeOr,    TypeCheckKind::tBool  },
  { BinaryOp::xorOp,   TypeCheckKind::tInt,   bcOpcodeXor,   TypeCheckKind::tInt   },
  { BinaryOp::xorOp,   TypeCheckKind::tBool,  bcOpcodeXor,   TypeCheckKind::tBool  },
  { BinaryOp::andOp,   TypeCheckKind::tInt,   bcOpcodeAnd,   TypeCheckKind::tInt   },
  { BinaryOp::andOp,   TypeCheckKind::tBool,  bcOpcodeAnd,   TypeCheckKind::tBool  },
  { BinaryOp::eq,      TypeCheckKind::tInt,   bcOpcodeCmpeq, TypeCheckKind::tBool  },
  { BinaryOp::eq,      TypeCheckKind::tFloat, bcOpcodeCmpeq, TypeCheckKind::tBool  },
  { BinaryOp::eq,      TypeCheckKind::tBool,  bcOpcodeCmpeq, TypeCheckKind::tBool  },
  { BinaryOp::eq,      TypeCheckKind::tEnum,  bcOpcodeCmpeq, TypeCheckKind::tBool  },
  { BinaryOp::ne,      TypeCheckKind::tInt,   bcOpcodeCmpne, TypeCheckKind::tBool  },
  { BinaryOp::ne,      TypeCheckKind::tFloat, bcOpcodeCmpne, TypeCheckKind::tBool  },
  { BinaryOp::ne,      TypeCheckKind::tBool,  bcOpcodeCmpne, TypeCheckKind::tBool  },
  { BinaryOp::ne,      TypeCheckKind::tEnum,  bcOpcodeCmpne, TypeCheckKind::tBool  },
  { BinaryOp::lt,      TypeCheckKind::tInt,   bcOpcodeCmplt, TypeCheckKind::tBool  },
  { BinaryOp::lt,      TypeCheckKind::tFloat, bcOpcodeCmplt, TypeCheckKind::tBool  },
  { BinaryOp::lt,      TypeCheckKind::tEnum,  bcOpcodeCmplt, TypeCheckKind::tBool  },
  { BinaryOp::gt,      TypeCheckKind::tInt,   bcOpcodeCmpgt, TypeCheckKind::tBool  },
  { BinaryOp::gt,      TypeCheckKind::tFloat, bcOpcodeCmpgt, TypeCheckKind::tBool  },
  { BinaryOp::gt,      TypeCheckKind::tEnum,  bcOpcodeCmpgt, TypeCheckKind::tBool  },
  { BinaryOp::le,      TypeCheckKind::tInt,   bcOpcodeCmple, TypeCheckKind::tBool  },
  { BinaryOp::le,      TypeCheckKind::tFloat, bcOpcodeCmple, TypeCheckKind::tBool  },
  { BinaryOp::le,      TypeCheckKind::tEnum,  bcOpcodeCmple, TypeCheckKind::tBool  },
  { BinaryOp::ge,      TypeCheckKind::tInt,   bcOpcodeCmpge, TypeCheckKind::tBool  },
  { BinaryOp::ge,      TypeCheckKind::tFloat, bcOpcodeCmpge, TypeCheckKind::tBool  },
  { BinaryOp::ge,      TypeCheckKind::tEnum,  bcOpcodeCmpge, TypeCheckKind::tBool  },
  { BinaryOp::shl,     TypeCheckKind::tInt,   bcOpcodeSll,   TypeCheckKind::tInt   },
  { BinaryOp::shr,     TypeCheckKind::tInt,   bcOpcodeSra,   TypeCheckKind::tInt   },
  { BinaryOp::add,     TypeCheckKind::tInt,   bcOpcodeAdd,   TypeCheckKind::tInt   },
  { BinaryOp::add,     TypeCheckKind::tFloat, bcOpcodeAdd,   TypeCheckKind::tFloat },
  { BinaryOp::sub,     TypeCheckKind::tInt,   bcOpcodeSub,   TypeCheckKind::tInt   },
  { BinaryOp::sub,     TypeCheckKind::tFloat, bcOpcodeSub,   TypeCheckKind::tFloat },
  { BinaryOp::mul,     TypeCheckKind::tInt,   bcOpcodeMul,   TypeCheckKind::tInt   },
  { BinaryOp::mul,     TypeCheckKind::tFloat, bcOpcodeMul,   TypeCheckKind::tFloat },
  { BinaryOp::div,     TypeCheckKind::tInt,   bcOpcodeDiv,   TypeCheckKind::tInt   },
  { BinaryOp::div,     TypeCheckKind::tFloat, bcOpcodeDiv,   TypeCheckKind::tFloat },
  { BinaryOp::mod,     TypeCheckKind::tInt,   bcOpcodeMod,   TypeCheckKind::tInt   }
};

struct UnaryOpInfo {
  UnaryOp op;			// operator
  TypeCheckKind in;		// operand type
  uint8_t opcode;		// instruction to use
  TypeCheckKind out;		// result type
};

static std::vector<UnaryOpInfo> unaryOpInfo {
  { UnaryOp::neg,   TypeCheckKind::tInt,   bcOpcodeNeg, TypeCheckKind::tInt   },
  { UnaryOp::neg,   TypeCheckKind::tFloat, bcOpcodeNeg, TypeCheckKind::tFloat },
  { UnaryOp::notOp, TypeCheckKind::tInt,   bcOpcodeNot, TypeCheckKind::tInt   },
  { UnaryOp::notOp, TypeCheckKind::tBool,  bcOpcodeNot, TypeCheckKind::tBool  }
};

//------------------------------------------------------------------------

ExprResult codeGenExpr(Expr *expr, Context &ctx, BytecodeFile &bcFunc) {
  switch (expr->kind()) {
  case Expr::Kind::binaryOpExpr:
    return codeGenBinaryOpExpr((BinaryOpExpr *)expr, ctx, bcFunc);
  case Expr::Kind::unaryOpExpr:
    return codeGenUnaryOpExpr((UnaryOpExpr *)expr, ctx, bcFunc);
  case Expr::Kind::propagateExpr:
    return codeGenPropagateExpr((PropagateExpr *)expr, ctx, bcFunc);
  case Expr::Kind::checkExpr:
    return codeGenCheckExpr((CheckExpr *)expr, ctx, bcFunc);
  case Expr::Kind::callExpr:
    return codeGenCallExpr((CallExpr *)expr, ctx, bcFunc);
  case Expr::Kind::memberExpr:
    return codeGenMemberExpr((MemberExpr *)expr, ctx, bcFunc);
  case Expr::Kind::indexExpr:
    return codeGenIndexExpr((IndexExpr *)expr, ctx, bcFunc);
  case Expr::Kind::parenExpr:
    return codeGenParenExpr((ParenExpr *)expr, ctx, bcFunc);
  case Expr::Kind::newExpr:
    return codeGenNewExpr((NewExpr *)expr, ctx, bcFunc);
  case Expr::Kind::makeExpr:
    return codeGenMakeExpr((MakeExpr *)expr, ctx, bcFunc);
  case Expr::Kind::funcPointerExpr:
    return codeGenFuncPointerExpr((FuncPointerExpr *)expr, ctx, bcFunc);
  case Expr::Kind::nilExpr:
    return codeGenNilExpr((NilExpr *)expr, ctx, bcFunc);
  case Expr::Kind::nilTestExpr:
    return codeGenNilTestExpr((NilTestExpr *)expr, ctx, bcFunc);
  case Expr::Kind::errorExpr:
    return codeGenErrorExpr((ErrorExpr *)expr, ctx, bcFunc);
  case Expr::Kind::validExpr:
    return codeGenValidExpr((ValidExpr *)expr, ctx, bcFunc);
  case Expr::Kind::okExpr:
    return codeGenOkExpr((OkExpr *)expr, ctx, bcFunc);
  case Expr::Kind::identExpr:
    return codeGenIdentExpr((IdentExpr *)expr, ctx, bcFunc);
  case Expr::Kind::litVectorExpr:
    return codeGenLitVectorExpr((LitVectorExpr *)expr, ctx, bcFunc);
  case Expr::Kind::litSetExpr:
    return codeGenLitSetExpr((LitSetExpr *)expr, ctx, bcFunc);
  case Expr::Kind::litMapExpr:
    return codeGenLitMapExpr((LitMapExpr *)expr, ctx, bcFunc);
  case Expr::Kind::litIntExpr:
    return codeGenLitIntExpr((LitIntExpr *)expr, ctx, bcFunc);
  case Expr::Kind::litFloatExpr:
    return codeGenLitFloatExpr((LitFloatExpr *)expr, ctx, bcFunc);
  case Expr::Kind::litBoolExpr:
    return codeGenLitBoolExpr((LitBoolExpr *)expr, ctx, bcFunc);
  case Expr::Kind::litCharExpr:
    return codeGenLitCharExpr((LitCharExpr *)expr, ctx, bcFunc);
  case Expr::Kind::litStringExpr:
    return codeGenLitStringExpr((LitStringExpr *)expr, ctx, bcFunc);
  case Expr::Kind::interpStringExpr:
    return codeGenInterpStringExpr((InterpStringExpr *)expr, ctx, bcFunc);
  default:
    error(expr->loc, "Internal error in codeGenExpr()");
    return ExprResult();
  }
}

static ExprResult codeGenBinaryOpExpr(BinaryOpExpr *expr, Context &ctx, BytecodeFile &bcFunc) {
  //--- left operand

  ExprResult lhsRes = codeGenExpr(expr->lhs.get(), ctx, bcFunc);
  if (!lhsRes.ok) {
    return ExprResult();
  }
  if (!lhsRes.type) {
    error(expr->lhs->loc, "Non-value on left side of binary operator");
    return ExprResult();
  }

  //--- special cases

  // conditional or/and
  if (expr->op == BinaryOp::condOr || expr->op == BinaryOp::condAnd) {
    if (!typeCheckBool(lhsRes.type.get())) {
      error(expr->lhs->loc, "Non-boolean on left side of conditional and/or");
      return ExprResult();
    }
    bcFunc.addGetStackInstr(0);
    uint32_t label = bcFunc.allocCodeLabel();
    uint8_t branchOpcode = (expr->op == BinaryOp::condOr) ? bcOpcodeBranchTrue
                                                          : bcOpcodeBranchFalse;
    bcFunc.addBranchInstr(branchOpcode, label);
    bcFunc.addInstr(bcOpcodePop);
    ExprResult rhsRes = codeGenExpr(expr->rhs.get(), ctx, bcFunc);
    if (!rhsRes.ok) {
      return ExprResult();
    }
    if (!rhsRes.type) {
      error(expr->rhs->loc, "Non-value on right side of binary operator");
      return ExprResult();
    }
    if (!typeCheckBool(rhsRes.type.get())) {
      error(expr->lhs->loc, "Non-boolean on right side of conditional and/or");
      return ExprResult();
    }
    bcFunc.setCodeLabel(label);
    return ExprResult(std::make_unique<CSimpleTypeRef>(expr->loc, ctx.boolType));
  }

  //--- right operand

  ExprResult rhsRes = codeGenExpr(expr->rhs.get(), ctx, bcFunc);
  if (!rhsRes.ok) {
    return ExprResult();
  }
  if (!rhsRes.type) {
    error(expr->rhs->loc, "Non-value on right side of binary operator");
    return ExprResult();
  }

  //--- special cases

  // string concat
  if (expr->op == BinaryOp::add &&
      typeCheckString(lhsRes.type.get()) && typeCheckString(rhsRes.type.get())) {
    bcFunc.addPushIInstr(2);
    bcFunc.addPushNativeInstr(mangleStringConcatFuncName());
    bcFunc.addInstr(bcOpcodeCall);
    return ExprResult(std::make_unique<CSimpleTypeRef>(expr->loc, ctx.stringType));
  }

  // string comparison
  if ((expr->op == BinaryOp::eq || expr->op == BinaryOp::ne ||
       expr->op == BinaryOp::lt || expr->op == BinaryOp::gt ||
       expr->op == BinaryOp::le || expr->op == BinaryOp::ge) &&
      typeCheckString(lhsRes.type.get()) && typeCheckString(rhsRes.type.get())) {
    bcFunc.addPushIInstr(2);
    bcFunc.addPushNativeInstr(mangleStringCompareFuncName());
    bcFunc.addInstr(bcOpcodeCall);
    bcFunc.addPushIInstr(0);
    switch (expr->op) {
    case BinaryOp::eq: bcFunc.addInstr(bcOpcodeCmpeq); break;
    case BinaryOp::ne: bcFunc.addInstr(bcOpcodeCmpne); break;
    case BinaryOp::lt: bcFunc.addInstr(bcOpcodeCmplt); break;
    case BinaryOp::gt: bcFunc.addInstr(bcOpcodeCmpgt); break;
    case BinaryOp::le: bcFunc.addInstr(bcOpcodeCmple); break;
    case BinaryOp::ge: bcFunc.addInstr(bcOpcodeCmpge); break;
    default: break;
    }
    return ExprResult(std::make_unique<CSimpleTypeRef>(expr->loc, ctx.boolType));
  }

  // same/not-same on pointers
  if ((expr->op == BinaryOp::same || expr->op == BinaryOp::notSame) &&
      typeCheckPointer(lhsRes.type.get()) && typeCheckPointer(rhsRes.type.get())) {
    if (!typeMatch(lhsRes.type.get(), rhsRes.type.get())) {
      error(expr->loc, "Mismatched pointer types in ===/!==");
      return ExprResult();
    }
    bcFunc.addInstr(expr->op == BinaryOp::same ? bcOpcodeCmpeq : bcOpcodeCmpne);
    return ExprResult(std::make_unique<CSimpleTypeRef>(expr->loc, ctx.boolType));
  }

  // function pointer apply
  if (expr->op == BinaryOp::mul &&
      typeCheckFuncPointer(lhsRes.type.get())) {
    CParamTypeRef *funcType = (CParamTypeRef *)lhsRes.type.get();
    size_t nArgs = funcType->params.size() - (funcType->hasReturnType ? 1 : 0);
    if (nArgs == 0 || !typeMatch(rhsRes.type.get(), funcType->params[0].get())) {
      error(expr->loc, "Type mismatch in function pointer apply operation");
      return ExprResult();
    }
    bcFunc.addPushIInstr(2);
    bcFunc.addPushNativeInstr("_allocFuncPtrApply");
    bcFunc.addInstr(bcOpcodeCall);
    std::vector<std::unique_ptr<CTypeRef>> params;
    for (size_t i = 1; i < funcType->params.size(); ++i) {
      params.push_back(std::unique_ptr<CTypeRef>(funcType->params[i]->copy()));
    }
    return ExprResult(std::make_unique<CParamTypeRef>(expr->loc, ctx.funcType,
						      funcType->hasReturnType,
						      std::move(params)));
  }

  //--- binary op table

  for (BinaryOpInfo &opInfo : binaryOpInfo) {
    if (opInfo.op == expr->op &&
	typeCheckOperand(lhsRes.type.get(), opInfo.in) &&
	typeCheckOperand(rhsRes.type.get(), opInfo.in)) {
      bcFunc.addInstr(opInfo.opcode);
      return ExprResult(makeResultType(opInfo.out, expr->loc, ctx));
    }
  }

  error(expr->loc, "Invalid operand types for binary operator");
  return ExprResult();
}

static ExprResult codeGenUnaryOpExpr(UnaryOpExpr *expr, Context &ctx, BytecodeFile &bcFunc) {
  //--- special cases

  // negative decimal integer literal -- this allows minInt to be
  // represented as a decimal value
  if (expr->op == UnaryOp::neg &&
      expr->expr->kind() == Expr::Kind::litIntExpr) {
    LitIntExpr *litIntExpr = (LitIntExpr *)expr->expr.get();
    if (litIntExpr->radix == 10) {
      return codeGenLitInt("-" + litIntExpr->val, 10, expr->loc, ctx, bcFunc);
    }
  }

  //--- generate code for the operand

  ExprResult exprRes = codeGenExpr(expr->expr.get(), ctx, bcFunc);
  if (!exprRes.ok) {
    return ExprResult();
  }
  if (!exprRes.type) {
    error(expr->expr->loc, "Non-value in unary operator");
    return ExprResult();
  }

  //--- special cases

  // container length
  if (expr->op == UnaryOp::length && typeCheckContainer(exprRes.type.get())) {
    std::vector<ExprResult> args;
    args.push_back(ExprResult(std::unique_ptr<CTypeRef>(exprRes.type->copy())));
    CFuncDecl *funcDecl = findFunction("length", args, ctx);
    if (!funcDecl) {
      error(expr->loc, "Internal: length operator");
      return ExprResult();
    }
    bcFunc.addPushIInstr(1);
    bcFunc.addPushNativeInstr(mangleFunctionName(funcDecl));
    bcFunc.addInstr(bcOpcodeCall);
    if (!funcDecl->returnType) {
      error(expr->loc, "Invalid length function");
      return ExprResult();
    }
    return ExprResult(std::unique_ptr<CTypeRef>(funcDecl->returnType->copy()));
  }

  // varstruct up-cast
  if (expr->op == UnaryOp::varstruct) {
    if (!typeCheckSubStruct(exprRes.type.get())) {
      error(expr->loc, "varstruct up-cast operator used on non-sub-struc");
      return ExprResult();
    }
    // this doesn't generate any code; it just changes the type
    CVarStructType *varStruct = ((CSubStructType *)exprRes.type->type)->parent;
    return ExprResult(std::make_unique<CSimpleTypeRef>(expr->loc, varStruct));
  }

  // substruct dispatch
  if (expr->op == UnaryOp::substruct) {
    error(expr->loc, "substruct operator used outside of function call");
    return ExprResult();
  }

  //--- unary op table

  for (UnaryOpInfo &opInfo : unaryOpInfo) {
    if (opInfo.op == expr->op &&
	typeCheckOperand(exprRes.type.get(), opInfo.in)) {
      bcFunc.addInstr(opInfo.opcode);
      return ExprResult(makeResultType(opInfo.out, expr->loc, ctx));
    }
  }

  error(expr->loc, "Invalid operand type for unary operator");
  return ExprResult();
}

static std::unique_ptr<CTypeRef> makeResultType(TypeCheckKind kind, Location loc, Context &ctx) {
  switch (kind) {
  case TypeCheckKind::tInt:   return std::make_unique<CSimpleTypeRef>(loc, ctx.intType);
  case TypeCheckKind::tFloat: return std::make_unique<CSimpleTypeRef>(loc, ctx.floatType);
  case TypeCheckKind::tBool:  return std::make_unique<CSimpleTypeRef>(loc, ctx.boolType);
  default:                    return nullptr;
  }
}

static ExprResult codeGenPropagateExpr(PropagateExpr *expr, Context &ctx, BytecodeFile &bcFunc) {
  ExprResult res = codeGenExpr(expr->expr.get(), ctx, bcFunc);
  if (!res.ok) {
    return ExprResult();
  }
  if (!res.type) {
    error(expr->loc, "Non-value used in '?' operator");
    return ExprResult();
  }
  if (!typeCheckResult(res.type.get())) {
    error(expr->loc, "Expression with '?' must be a Result[T]");
    return ExprResult();
  }
  CParamTypeRef *resultTypeRef = (CParamTypeRef *)res.type.get();
  std::unique_ptr<CTypeRef> param;
  if (resultTypeRef->params.size() == 1) {
    param = std::unique_ptr<CTypeRef>(resultTypeRef->params[0]->copy());
  }

  if (!typeCheckResult(ctx.returnType)) {
    error(expr->loc, "The '?' operator can only be used in functions returning Result[T]");
    return ExprResult();
  }

  if (param) {
    bcFunc.addGetStackInstr(0);
  }
  bcFunc.addInstr(bcOpcodeTestValid);
  uint32_t label = bcFunc.allocCodeLabel();
  bcFunc.addBranchInstr(bcOpcodeBranchTrue, label);
  bcFunc.addInstr(bcOpcodePushError);
  bcFunc.addInstr(bcOpcodeReturn);
  bcFunc.setCodeLabel(label);

  return ExprResult(std::move(param));
}

static ExprResult codeGenCheckExpr(CheckExpr *expr, Context &ctx, BytecodeFile &bcFunc) {
  ExprResult res = codeGenExpr(expr->expr.get(), ctx, bcFunc);
  if (!res.ok) {
    return ExprResult();
  }
  if (!res.type) {
    error(expr->loc, "Non-value used in '!' operator");
    return ExprResult();
  }
  if (!typeCheckResult(res.type.get())) {
    error(expr->loc, "Expression with '!' must be a Result[T]");
    return ExprResult();
  }
  CParamTypeRef *resultTypeRef = (CParamTypeRef *)res.type.get();
  std::unique_ptr<CTypeRef> param;
  if (resultTypeRef->params.size() == 1) {
    param = std::unique_ptr<CTypeRef>(resultTypeRef->params[0]->copy());
  }

  bcFunc.addInstr(bcOpcodeCheckValid);
  if (!param) {
    bcFunc.addInstr(bcOpcodePop);
  }

  return ExprResult(std::move(param));
}

static ExprResult codeGenCallExpr(CallExpr *expr, Context &ctx, BytecodeFile &bcFunc) {
  //--- generate code for the args
  std::vector<ExprResult> argResults;
  size_t substructArgIdx = expr->args.size();
  for (size_t argIdx = 0; argIdx < expr->args.size(); ++argIdx) {
    Expr *arg = expr->args[argIdx].get();
    if (arg->kind() == Expr::Kind::unaryOpExpr &&
	((UnaryOpExpr *)arg)->op == UnaryOp::substruct) {
      if (substructArgIdx < expr->args.size()) {
	error(arg->loc, "More than one substruct arg in a function call expression");
	return ExprResult();
      }
      substructArgIdx = argIdx;
      arg = ((UnaryOpExpr *)arg)->expr.get();
    }
    ExprResult res = codeGenExpr(arg, ctx, bcFunc);
    if (!res.ok) {
      return ExprResult();
    }
    if (!res.type) {
      error(arg->loc, "Non-value used as function argument");
      return ExprResult();
    }
    argResults.push_back(std::move(res));
  }

  //--- push the arg count
  bcFunc.addPushIInstr((int64_t)argResults.size());

  //--- regular function call
  std::unique_ptr<CTypeRef> returnType;
  if (expr->func->kind() == Expr::Kind::identExpr) {
    std::string name = ((IdentExpr *)expr->func.get())->name;
    ExprResult res;
    if (substructArgIdx < expr->args.size()) {
      res = codeGenFindSubstructFunc(name, argResults, substructArgIdx, expr->func->loc,
				     ctx, bcFunc);
    } else {
      res = codeGenFindFunc(name, argResults, expr->func->loc, ctx, bcFunc);
    }
    if (!res.ok) {
      return res;
    }
    returnType = std::move(res.type);
    bcFunc.addInstr(bcOpcodeCall);

  //--- function pointer call
  } else if (expr->func->kind() == Expr::Kind::parenExpr) {
    if (substructArgIdx < expr->args.size()) {
      error(expr->loc, "Substruct args cannot be used in function pointer calls");
      return ExprResult();
    }
    ExprResult funcRes = codeGenExpr(((ParenExpr *)expr->func.get())->expr.get(), ctx, bcFunc);
    if (!funcRes.ok) {
      return ExprResult();
    }
    if (!funcRes.type) {
      error(expr->func->loc, "Non-value used as function pointer");
      return ExprResult();
    }
    if (!typeCheckFuncPointer(funcRes.type.get())) {
      error(expr->func->loc, "Non-function-pointer used in function pointer call");
      return ExprResult();
    }
    CParamTypeRef *funcType = (CParamTypeRef *)funcRes.type.get();
    size_t nArgs = funcType->params.size() - (funcType->hasReturnType ? 1 : 0);
    if (argResults.size() != nArgs) {
      error(expr->loc, "Incorrect number of arguments in function pointer call");
      return ExprResult();
    }
    for (size_t i = 0; i < nArgs; ++i) {
      if (!typeMatch(argResults[i].type.get(), funcType->params[i].get())) {
	error(expr->args[i]->loc, "Incorrect argument type in function pointer call");
	return ExprResult();
      }
    }
    if (funcType->hasReturnType) {
      returnType = std::unique_ptr<CTypeRef>(funcType->params.back()->copy());
    }
    bcFunc.addInstr(bcOpcodePtrcall);

  //--- invalid call
  } else {
    error(expr->func->loc, "Function in a call expression must be an identifier or parenthesized function pointer");
    return ExprResult();
  }

  //--- pop an unused return value
  if (!returnType) {
    // the bytecode return instruction always returns a value
    bcFunc.addInstr(bcOpcodePop);
  }

  return ExprResult(std::move(returnType));
}

static ExprResult codeGenFindFunc(const std::string &name, std::vector<ExprResult> &argResults,
				  Location loc, Context &ctx, BytecodeFile &bcFunc) {
  //--- find the function
  CFuncDecl *funcDecl = findFunction(name, argResults, ctx);
  if (!funcDecl) {
    std::string msg = "Function " + name + "(";
    for (size_t i = 0; i < argResults.size(); ++i) {
      if (i > 0) {
	msg += ",";
      }
      msg += argResults[i].type->toString();
    }
    msg += ") not found";
    error(loc, msg.c_str());
    return ExprResult();
  }

  //--- push the function address
  if (funcDecl->native) {
    bcFunc.addPushNativeInstr(mangleFunctionName(funcDecl));
  } else {
    bcFunc.addPushBcodeInstr(mangleFunctionName(funcDecl));
  }

  //--- get the return type
  std::unique_ptr<CTypeRef> returnType;
  if (funcDecl->returnType) {
    returnType = std::unique_ptr<CTypeRef>(funcDecl->returnType->copy());
  }

  return ExprResult(std::move(returnType));
}

static ExprResult codeGenFindSubstructFunc(const std::string &name,
					   std::vector<ExprResult> &argResults,
					   size_t substructArgIdx,
					   Location loc, Context &ctx, BytecodeFile &bcFunc) {
  //--- check for a varstruct
  if (!typeCheckVarStruct(argResults[substructArgIdx].type.get())) {
    error(loc, "Substruct arg in call is not a varstruct");
    return ExprResult();
  }
  CVarStructType *varType = (CVarStructType *)argResults[substructArgIdx].type->type;

  //--- read the ID field
  bcFunc.addGetStackInstr((uint32_t)(argResults.size() - substructArgIdx));
  bcFunc.addPushIInstr(0);
  bcFunc.addInstr(bcOpcodeLoad);

  //--- copy argResults
  std::vector<ExprResult> argResults2;
  for (ExprResult &res : argResults) {
    argResults2.push_back(ExprResult(std::unique_ptr<CTypeRef>(res.type->copy())));
  }

  //--- handle the cases
  std::unique_ptr<CTypeRef> returnType;
  uint32_t endLabel = bcFunc.allocCodeLabel();
  for (size_t i = 0; i < varType->subStructs.size(); ++i) {

    //--- compare the substruct ID
    CSubStructType *subType = varType->subStructs[i];
    bcFunc.addGetStackInstr(0);    // copy the ID
    bcFunc.addPushIInstr(subType->id);
    bcFunc.addInstr(bcOpcodeCmpeq);
    uint32_t nextLabel = bcFunc.allocCodeLabel();
    bcFunc.addBranchInstr(bcOpcodeBranchFalse, nextLabel);
    bcFunc.addInstr(bcOpcodePop);  // pop the ID

    //--- find the function for this substruct
    argResults2[substructArgIdx] = ExprResult(std::make_unique<CSimpleTypeRef>(loc, subType));
    ExprResult res = codeGenFindFunc(name, argResults2, loc, ctx, bcFunc);
    if (!res.ok) {
      return ExprResult();
    }

    //--- check return type
    if (i == 0) {
      returnType = std::move(res.type);
    } else {
      if ((returnType && !res.type) ||
	  (!returnType && res.type) ||
	  (returnType && res.type && !typeMatch(returnType.get(), res.type.get()))) {
	error(loc, "Type mismatch in substruct functions");
	return ExprResult();
      }
    }

    //--- next case
    bcFunc.addBranchInstr(bcOpcodeBranch, endLabel);
    bcFunc.setCodeLabel(nextLabel);
  }

  //--- default: invalid substruct - force a nil call
  bcFunc.addInstr(bcOpcodePop);  // pop the ID
  bcFunc.addInstr(bcOpcodePushNil);

  //--- end of cases
  bcFunc.setCodeLabel(endLabel);

  return ExprResult(std::move(returnType));
}

CFuncDecl *findFunction(const std::string &name, std::vector<ExprResult> &argResults,
			Context &ctx) {
  auto range = ctx.funcs.equal_range(name);
  for (auto iter = range.first; iter != range.second; ++iter) {
    CFuncDecl *funcDecl = iter->second.get();
    if (functionMatch(argResults, funcDecl)) {
      if (!ctx.moduleIsVisible(funcDecl->module)) {
	return nullptr;
      }
      for (std::unique_ptr<CArg> &arg : funcDecl->args) {
	if (!ctx.moduleIsVisible(arg->type->type->module)) {
	  return nullptr;
	}
      }
      if (funcDecl->returnType &&
	  !ctx.moduleIsVisible(funcDecl->returnType->type->module)) {
	return nullptr;
      }
      return funcDecl;
    }
  }
  return nullptr;
}

static ExprResult codeGenMemberExpr(MemberExpr *expr, Context &ctx, BytecodeFile &bcFunc) {
  //--- enum member

  if (expr->lhs->kind() == Expr::Kind::identExpr) {
    IdentExpr *identExpr = (IdentExpr *)expr->lhs.get();
    CType *type = ctx.findType(identExpr->name);
    if (type) {
      if (type->kind() != CTypeKind::enumType) {
	error(expr->loc, "Non-enum type '%s' in enum member expression", identExpr->name.c_str());
	return ExprResult();
      }
      CEnumType *enumType = (CEnumType *)type;
      auto iter = enumType->members.find(expr->member);
      if (iter == enumType->members.end()) {
	error(expr->loc, "Undefined enum member '%s.%s'",
	      identExpr->name.c_str(), expr->member.c_str());
	return ExprResult();
      }
      int memberIdx = iter->second;
      bcFunc.addPushIInstr((int64_t)memberIdx);
      return ExprResult(std::make_unique<CSimpleTypeRef>(expr->loc, enumType));
    }
  }

  //--- struct member

  ExprResult res = codeGenExpr(expr->lhs.get(), ctx, bcFunc);
  if (!res.ok) {
    return ExprResult();
  }
  if (!res.type) {
    error(expr->loc, "Non-value used in member expression");
    return ExprResult();
  }

  CField *field = findField(res.type.get(), expr->member, expr->loc, "member expression");
  if (!field) {
    return ExprResult();
  }

  bcFunc.addPushIInstr(field->fieldIdx);
  bcFunc.addInstr(bcOpcodeLoad);

  return ExprResult(std::unique_ptr<CTypeRef>(field->type->copy()));
}

CField *findField(CTypeRef *objType, const std::string &fieldName, Location &loc, const char *use) {
  if (typeCheckStruct(objType)) {
    CStructType *type = (CStructType *)objType->type;
    auto iter = type->fields.find(fieldName);
    if (iter == type->fields.end()) {
      error(loc, "'%s' is not a field of struct type '%s'",
	    fieldName.c_str(), type->name.c_str());
      return nullptr;
    }
    return iter->second.get();

  } else if (typeCheckVarStruct(objType)) {
    CVarStructType *type = (CVarStructType *)objType->type;
    auto iter = type->fields.find(fieldName);
    if (iter == type->fields.end()) {
      error(loc, "'%s' is not a field of varstruct type '%s'",
	    fieldName.c_str(), type->name.c_str());
      return nullptr;
    }
    return iter->second.get();

  } else if (typeCheckSubStruct(objType)) {
    CSubStructType *type = (CSubStructType *)objType->type;
    auto subIter = type->fields.find(fieldName);
    if (subIter != type->fields.end()) {
      return subIter->second.get();
    }
    auto varIter = type->parent->fields.find(fieldName);
    if (varIter == type->parent->fields.end()) {
      error(loc, "'%s' is not a field of substruct type '%s'",
	    fieldName.c_str(), type->name.c_str());
      return nullptr;
    }
    return varIter->second.get();

  } else {
    error(loc, "Non-struct used in %s", use);
    return nullptr;
  }
}

static ExprResult codeGenIndexExpr(IndexExpr *expr, Context &ctx, BytecodeFile &bcFunc) {
  ExprResult objRes = codeGenExpr(expr->obj.get(), ctx, bcFunc);
  if (!objRes.ok) {
    return ExprResult();
  }
  if (!objRes.type) {
    error(expr->obj->loc, "Non-value as object in index operator");
    return ExprResult();
  }
  if (!(typeCheckVector(objRes.type.get()) ||
	typeCheckMap(objRes.type.get()))) {
    error(expr->obj->loc, "Invalid type for object in index operator");
    return ExprResult();
  }

  ExprResult idxRes = codeGenExpr(expr->idx.get(), ctx, bcFunc);
  if (!idxRes.ok) {
    return ExprResult();
  }
  if (!idxRes.type) {
    error(expr->idx->loc, "Non-value as index in index operator");
    return ExprResult();
  }

  std::vector<ExprResult> args;
  args.push_back(ExprResult(std::unique_ptr<CTypeRef>(objRes.type->copy())));
  args.push_back(ExprResult(std::unique_ptr<CTypeRef>(idxRes.type->copy())));

  CFuncDecl *funcDecl = findFunction("get", args, ctx);
  if (!funcDecl) {
    error(expr->idx->loc, "Invalid type for index in index operator");
    return ExprResult();
  }

  bcFunc.addPushIInstr(2);
  bcFunc.addPushNativeInstr(mangleFunctionName(funcDecl));
  bcFunc.addInstr(bcOpcodeCall);
  if (!funcDecl->returnType) {
    error(expr->loc, "Invalid 'get' function");
    return ExprResult();
  }
  return ExprResult(std::unique_ptr<CTypeRef>(funcDecl->returnType->copy()));
}

static ExprResult codeGenParenExpr(ParenExpr *expr, Context &ctx, BytecodeFile &bcFunc) {
  return codeGenExpr(expr->expr.get(), ctx, bcFunc);
}

static ExprResult codeGenNewExpr(NewExpr *expr, Context &ctx, BytecodeFile &bcFunc) {
  std::unique_ptr<CTypeRef> typeRef = convertTypeRef(expr->type.get(), ctx);
  if (!typeRef) {
    return ExprResult();
  }
  if (typeCheckStringBuf(typeRef.get())) {
    return codeGenNewStringBufExpr(expr, std::move(typeRef), ctx, bcFunc);
  } else if (typeCheckContainer(typeRef.get())) {
    return codeGenNewContainerExpr(expr, std::move(typeRef), (CContainerType *)typeRef->type,
				   ctx, bcFunc);
  } else {
    error(expr->loc, "Invalid type in 'new' (must be StringBuf or container)");
    return ExprResult();
  }
}

static ExprResult codeGenNewStringBufExpr(NewExpr *expr, std::unique_ptr<CTypeRef> typeRef,
					  Context &ctx, BytecodeFile &bcFunc) {
  bcFunc.addPushIInstr(0);
  bcFunc.addPushNativeInstr("_allocStringBuf");
  bcFunc.addInstr(bcOpcodeCall);
  return ExprResult(std::move(typeRef));
}

static ExprResult codeGenNewContainerExpr(NewExpr *expr, std::unique_ptr<CTypeRef> typeRef,
					  CContainerType *type,
					  Context &ctx, BytecodeFile &bcFunc) {
  bcFunc.addPushIInstr(0);
  switch (type->kind()) {
  case CTypeKind::vectorType:
    bcFunc.addPushNativeInstr("_allocVector");
    break;
  case CTypeKind::setType:
    bcFunc.addPushNativeInstr("_allocSet");
    break;
  case CTypeKind::mapType:
    bcFunc.addPushNativeInstr("_allocMap");
    break;
  default:
    error(expr->loc, "Internal: codeGenNewContainerExpr");
    return ExprResult();
  }
  bcFunc.addInstr(bcOpcodeCall);
  return ExprResult(std::move(typeRef));
}

static ExprResult codeGenMakeExpr(MakeExpr *expr, Context &ctx, BytecodeFile &bcFunc) {
  std::unique_ptr<CTypeRef> typeRef = convertTypeRef(expr->type.get(), ctx);
  if (!typeRef) {
    return ExprResult();
  }
  if (typeCheckStruct(typeRef.get())) {
    CStructType *type = (CStructType *)typeRef->type;
    return codeGenMakeStructExpr(expr, std::move(typeRef), type, ctx, bcFunc);
  } else if (typeCheckVarStruct(typeRef.get())) {
    error(expr->loc, "Varstruct type in 'make'");
    return ExprResult();
  } else if (typeCheckSubStruct(typeRef.get())) {
    CSubStructType *type = (CSubStructType *)typeRef->type;
    return codeGenMakeSubStructExpr(expr, std::move(typeRef), type, ctx, bcFunc);
  } else {
    error(expr->loc, "Invalid type in 'make' (must be struct or substruct)");
    return ExprResult();
  }
}

static ExprResult codeGenMakeStructExpr(MakeExpr *expr, std::unique_ptr<CTypeRef> typeRef,
					CStructType *type, Context &ctx, BytecodeFile &bcFunc) {
  //--- allocate memory
  bcFunc.addPushIInstr((int64_t)type->fields.size());
  bcFunc.addPushIInstr(1);
  bcFunc.addPushNativeInstr("_allocStruct");
  bcFunc.addInstr(bcOpcodeCall);

  //--- initialize fields
  std::unordered_set<std::string> fieldHasInit;
  for (std::unique_ptr<FieldInit> &fieldInit : expr->fieldInits) {

    //--- find the field
    auto iter = type->fields.find(fieldInit->name);
    if (iter == type->fields.end()) {
      error(fieldInit->loc, "'%s' is not a field of struct type '%s'",
	    fieldInit->name.c_str(), type->name.c_str());
      return ExprResult();
    }
    CField *field = iter->second.get();

    //--- evaluate the initializer expression
    ExprResult res = codeGenExpr(fieldInit->val.get(), ctx, bcFunc);
    if (!res.ok) {
      return ExprResult();
    }
    if (!res.type) {
      error(fieldInit->val->loc, "Non-value used in struct initializer");
      return ExprResult();
    }
    if (!typeMatch(res.type.get(), field->type.get())) {
      error(fieldInit->val->loc, "Type mismatch in struct initializer");
      return ExprResult();
    }

    //--- store the field
    bcFunc.addGetStackInstr(1);
    bcFunc.addPushIInstr(field->fieldIdx);
    bcFunc.addInstr(bcOpcodeStore);

    fieldHasInit.insert(field->name);
  }

  //--- check that all fields were initialized
  bool ok = true;
  for (auto &pair : type->fields) {
    if (!fieldHasInit.count(pair.second->name)) {
      error(expr->loc, "Missing initializer for field '%s'", pair.second->name.c_str());
      ok = false;
    }
  }
  if (!ok) {
    return ExprResult();
  }

  return ExprResult(std::move(typeRef));
}

static ExprResult codeGenMakeSubStructExpr(MakeExpr *expr, std::unique_ptr<CTypeRef> typeRef,
					   CSubStructType *type, Context &ctx,
					   BytecodeFile &bcFunc) {
  //--- allocate memory
  bcFunc.addPushIInstr((int64_t)(1 + type->parent->fields.size() + type->fields.size()));
  bcFunc.addPushIInstr(1);
  bcFunc.addPushNativeInstr("_allocStruct");
  bcFunc.addInstr(bcOpcodeCall);

  //--- initialize the ID field
  bcFunc.addPushIInstr(type->id);
  bcFunc.addGetStackInstr(1);
  bcFunc.addPushIInstr(0);
  bcFunc.addInstr(bcOpcodeStore);

  //--- initialize fields
  std::unordered_set<std::string> fieldHasInit;
  for (std::unique_ptr<FieldInit> &fieldInit : expr->fieldInits) {

    //--- find the field
    CField *field = findField(typeRef.get(), fieldInit->name, expr->loc, "'make' expression");
    if (!field) {
      return ExprResult();
    }

    //--- evaluate the initializer expression
    ExprResult res = codeGenExpr(fieldInit->val.get(), ctx, bcFunc);
    if (!res.ok) {
      return ExprResult();
    }
    if (!res.type) {
      error(fieldInit->val->loc, "Non-value used in struct initializer");
      return ExprResult();
    }
    if (!typeMatch(res.type.get(), field->type.get())) {
      error(fieldInit->val->loc, "Type mismatch in struct initializer");
      return ExprResult();
    }

    //--- store the field
    bcFunc.addGetStackInstr(1);
    bcFunc.addPushIInstr(field->fieldIdx);
    bcFunc.addInstr(bcOpcodeStore);

    fieldHasInit.insert(field->name);
  }

  //--- check that all fields were initialized
  bool ok = true;
  for (auto &pair : type->fields) {
    if (!fieldHasInit.count(pair.second->name)) {
      error(expr->loc, "Missing initializer for field '%s'", pair.second->name.c_str());
      ok = false;
    }
  }
  for (auto &pair : type->parent->fields) {
    if (!fieldHasInit.count(pair.second->name)) {
      error(expr->loc, "Missing initializer for field '%s'", pair.second->name.c_str());
      ok = false;
    }
  }
  if (!ok) {
    return ExprResult();
  }

  return ExprResult(std::move(typeRef));
}

static ExprResult codeGenFuncPointerExpr(FuncPointerExpr *expr, Context &ctx,
					 BytecodeFile &bcFunc) {
  //--- find the function
  std::vector<ExprResult> cArgTypes;
  for (std::unique_ptr<TypeRef> &argType : expr->argTypes) {
    std::unique_ptr<CTypeRef> cArgType = convertTypeRef(argType.get(), ctx);
    if (!cArgType) {
      return ExprResult();
    }
    cArgTypes.push_back(ExprResult(std::move(cArgType)));
  }
  CFuncDecl *funcDecl = findFunction(expr->name, cArgTypes, ctx);
  if (!funcDecl) {
    error(expr->loc, "Undefined function '%s'", expr->name.c_str());
    return ExprResult();
  }

  //--- push the function address
  if (funcDecl->native) {
    bcFunc.addPushNativeInstr(mangleFunctionName(funcDecl));
  } else {
    bcFunc.addPushBcodeInstr(mangleFunctionName(funcDecl));
  }

  //--- call _allocFuncPtr
  bcFunc.addPushIInstr(1);
  bcFunc.addPushNativeInstr("_allocFuncPtr");
  bcFunc.addInstr(bcOpcodeCall);

  //--- construct the function pointer type
  std::vector<std::unique_ptr<CTypeRef>> params;
  for (ExprResult &res : cArgTypes) {
    params.push_back(std::move(res.type));
  }
  if (funcDecl->returnType) {
    params.push_back(std::unique_ptr<CTypeRef>(funcDecl->returnType->copy()));
  }
  return ExprResult(std::make_unique<CParamTypeRef>(expr->loc, ctx.funcType,
						    (bool)funcDecl->returnType,
						    std::move(params)));
}

static ExprResult codeGenNilExpr(NilExpr *expr, Context &ctx, BytecodeFile &bcFunc) {
  std::unique_ptr<CTypeRef> typeRef = convertTypeRef(expr->type.get(), ctx);
  if (!typeRef) {
    return ExprResult();
  }
  if (!typeCheckPointer(typeRef.get())) {
    error(expr->loc, "Only pointer types can be nil");
    return ExprResult();
  }
  bcFunc.addInstr(bcOpcodePushNil);
  return ExprResult(std::move(typeRef));
}

static ExprResult codeGenNilTestExpr(NilTestExpr *expr, Context &ctx, BytecodeFile &bcFunc) {
  ExprResult res = codeGenExpr(expr->expr.get(), ctx, bcFunc);
  if (!res.ok) {
    return ExprResult();
  }
  if (!res.type) {
    error(expr->loc, "Non-value used in nil() construct");
    return ExprResult();
  }
  if (!typeCheckPointer(res.type.get())) {
    error(expr->loc, "Argument to nil() must be a pointer");
    return ExprResult();
  }
  bcFunc.addInstr(bcOpcodePushNil);
  bcFunc.addInstr(bcOpcodeCmpeq);
  return ExprResult(std::make_unique<CSimpleTypeRef>(expr->loc, ctx.boolType));
}

static ExprResult codeGenErrorExpr(ErrorExpr *expr, Context &ctx, BytecodeFile &bcFunc) {
  std::vector<std::unique_ptr<CTypeRef>> params;
  if (expr->type) {
    std::unique_ptr<CTypeRef> typeRef = convertTypeRef(expr->type.get(), ctx);
    if (!typeRef) {
      return ExprResult();
    }
    params.push_back(std::move(typeRef));
  }
  bcFunc.addInstr(bcOpcodePushError);
  return ExprResult(std::make_unique<CParamTypeRef>(expr->loc, ctx.resultType,
						    false, std::move(params)));
}

static ExprResult codeGenValidExpr(ValidExpr *expr, Context &ctx, BytecodeFile &bcFunc) {
  std::vector<std::unique_ptr<CTypeRef>> params;
  if (expr->expr) {
    ExprResult res = codeGenExpr(expr->expr.get(), ctx, bcFunc);
    if (!res.ok) {
      return ExprResult();
    }
    if (!res.type) {
      error(expr->loc, "Non-value used in valid() construct");
      return ExprResult();
    }
    params.push_back(std::move(res.type));
  }
  // this doesn't generate any code - just leave the value (if any) as-is
  return ExprResult(std::make_unique<CParamTypeRef>(expr->loc, ctx.resultType,
						    false, std::move(params)));
}

static ExprResult codeGenOkExpr(OkExpr *expr, Context &ctx, BytecodeFile &bcFunc) {
  ExprResult res = codeGenExpr(expr->expr.get(), ctx, bcFunc);
  if (!res.ok) {
    return ExprResult();
  }
  if (!res.type) {
    error(expr->loc, "Non-value used in ok() construct");
    return ExprResult();
  }
  if (!typeCheckResult(res.type.get())) {
    error(expr->loc, "Argument to ok() must be a Result[T]");
    return ExprResult();
  }
  bcFunc.addInstr(bcOpcodeTestValid);
  return ExprResult(std::make_unique<CSimpleTypeRef>(expr->loc, ctx.boolType));
}

static ExprResult codeGenIdentExpr(IdentExpr *expr, Context &ctx, BytecodeFile &bcFunc) {
  return codeGenIdent(expr->name, expr->loc, ctx, bcFunc);
}

static ExprResult codeGenIdent(const std::string &ident, Location loc,
			       Context &ctx, BytecodeFile &bcFunc) {
  CSymbol *sym = ctx.findSymbol(ident);
  if (!sym) {
    error(loc, "Undefined symbol '%s'", ident.c_str());
    return ExprResult();
  }

  switch (sym->kind()) {

  case CSymbolKind::constant:
    return codeGenConstValue(((CConst *)sym)->value.get(), loc, ctx, bcFunc);

  case CSymbolKind::arg:
    bcFunc.addPushIInstr(((CArg *)sym)->argIdx);
    bcFunc.addInstr(bcOpcodeGetArg);
    return ExprResult(std::unique_ptr<CTypeRef>(sym->type->copy()));

  case CSymbolKind::var:
    bcFunc.addPushIInstr(((CVar *)sym)->frameIdx);
    bcFunc.addInstr(bcOpcodeGetVar);
    return ExprResult(std::unique_ptr<CTypeRef>(sym->type->copy()));

  default:
    error(loc, "Internal: codeGenIdent");
    return ExprResult();
  }
}

static ExprResult codeGenConstValue(CConstValue *val, Location loc,
				    Context &ctx, BytecodeFile &bcFunc) {
  if (val->isInt()) {
    bcFunc.addPushIInstr(((CConstIntValue *)val)->val);
    return ExprResult(std::make_unique<CSimpleTypeRef>(loc, ctx.intType));
  } else if (val->isFloat()) {
    bcFunc.addPushFInstr(((CConstFloatValue *)val)->val);
    return ExprResult(std::make_unique<CSimpleTypeRef>(loc, ctx.floatType));
  } else if (val->isBool()) {
    bcFunc.addInstr(((CConstBoolValue *)val)->val ? bcOpcodePushTrue : bcOpcodePushFalse);
    return ExprResult(std::make_unique<CSimpleTypeRef>(loc, ctx.boolType));
  } else if (val->isString()) {
    if (!codeGenString(((CConstStringValue *)val)->val, loc, bcFunc)) {
      return ExprResult();
    }
    return ExprResult(std::make_unique<CSimpleTypeRef>(loc, ctx.stringType));
  } else {
    error(loc, "Internal (codeGenConstValue)");
    return ExprResult();
  }
}

static ExprResult codeGenLitVectorExpr(LitVectorExpr *expr, Context &ctx, BytecodeFile &bcFunc) {
  bcFunc.addPushIInstr(0);
  bcFunc.addPushNativeInstr("_allocVector");
  bcFunc.addInstr(bcOpcodeCall);

  std::unique_ptr<CTypeRef> elemType;
  for (size_t i = 0; i < expr->vals.size(); ++i) {
    bcFunc.addGetStackInstr(0);
    ExprResult res = codeGenExpr(expr->vals[i].get(), ctx, bcFunc);
    if (!res.ok) {
      return ExprResult();
    }
    if (!res.type) {
      error(expr->vals[i]->loc, "Non-value used in vector literal");
      return ExprResult();
    }
    if (i == 0) {
      elemType = std::move(res.type);
    } else {
      if (!typeMatch(res.type.get(), elemType.get())) {
	error(expr->vals[i]->loc, "Elements in Vector literal are not all the same type");
	return ExprResult();
      }
    }
    bcFunc.addPushIInstr(2);
    bcFunc.addPushNativeInstr(mangleVectorAppendFuncName());
    bcFunc.addInstr(bcOpcodeCall);
    bcFunc.addInstr(bcOpcodePop);
  }

  std::vector<std::unique_ptr<CTypeRef>> params;
  params.push_back(std::move(elemType));
  std::unique_ptr<CParamTypeRef> type = std::make_unique<CParamTypeRef>(expr->loc, ctx.vectorType,
									false, std::move(params));
  instantiateTypeRef(type.get(), ctx);
  return ExprResult(std::move(type));
}

static ExprResult codeGenLitSetExpr(LitSetExpr *expr, Context &ctx, BytecodeFile &bcFunc) {
  bcFunc.addPushIInstr(0);
  bcFunc.addPushNativeInstr("_allocSet");
  bcFunc.addInstr(bcOpcodeCall);

  std::unique_ptr<CTypeRef> elemType;
  std::string insertFuncName;
  for (size_t i = 0; i < expr->vals.size(); ++i) {
    bcFunc.addGetStackInstr(0);
    ExprResult res = codeGenExpr(expr->vals[i].get(), ctx, bcFunc);
    if (!res.ok) {
      return ExprResult();
    }
    if (!res.type) {
      error(expr->vals[i]->loc, "Non-value used in set literal");
      return ExprResult();
    }
    if (i == 0) {
      if (typeCheckString(res.type.get()) ||
	  typeCheckInt(res.type.get())) {
	insertFuncName = mangleSetInsertFuncName(res.type.get());
      } else {
	error(expr->loc, "Set element type must be String or Int");
	return ExprResult();
      }
      elemType = std::move(res.type);
    } else {
      if (!typeMatch(res.type.get(), elemType.get())) {
	error(expr->vals[i]->loc, "Elements in Set literal are not all the same type");
	return ExprResult();
      }
    }
    bcFunc.addPushIInstr(2);
    bcFunc.addPushNativeInstr(insertFuncName);
    bcFunc.addInstr(bcOpcodeCall);
    bcFunc.addInstr(bcOpcodePop);
  }

  std::vector<std::unique_ptr<CTypeRef>> params;
  params.push_back(std::move(elemType));
  std::unique_ptr<CParamTypeRef> type = std::make_unique<CParamTypeRef>(expr->loc, ctx.setType,
									false, std::move(params));
  instantiateTypeRef(type.get(), ctx);
  return ExprResult(std::move(type));
}

static ExprResult codeGenLitMapExpr(LitMapExpr *expr, Context &ctx, BytecodeFile &bcFunc) {
  bcFunc.addPushIInstr(0);
  bcFunc.addPushNativeInstr("_allocMap");
  bcFunc.addInstr(bcOpcodeCall);

  std::unique_ptr<CTypeRef> keyType, valueType;
  std::string setFuncName;
  for (size_t i = 0; i < expr->pairs.size(); ++i) {
    Expr *keyExpr = expr->pairs[i].first.get();
    Expr *valueExpr = expr->pairs[i].second.get();
    bcFunc.addGetStackInstr(0);
    ExprResult keyRes = codeGenExpr(keyExpr, ctx, bcFunc);
    if (!keyRes.ok) {
      return ExprResult();
    }
    if (!keyRes.type) {
      error(keyExpr->loc, "Non-value used as key in map literal");
      return ExprResult();
    }
    ExprResult valueRes = codeGenExpr(valueExpr, ctx, bcFunc);
    if (!valueRes.ok) {
      return ExprResult();
    }
    if (!valueRes.type) {
      error(valueExpr->loc, "Non-value used as value in map literal");
      return ExprResult();
    }
    if (i == 0) {
      if (typeCheckString(keyRes.type.get()) ||
	  typeCheckInt(keyRes.type.get())) {
	setFuncName = mangleMapSetFuncName(keyRes.type.get());
      } else {
	error(expr->loc, "Map key type must be String or Int");
	return ExprResult();
      }
      keyType = std::move(keyRes.type);
      valueType = std::move(valueRes.type);
    } else {
      if (!typeMatch(keyRes.type.get(), keyType.get())) {
	error(keyExpr->loc, "Keys in Map literal are not all the same type");
	return ExprResult();
      }
      if (!typeMatch(valueRes.type.get(), valueType.get())) {
	error(valueExpr->loc, "Values in Map literal are not all the same type");
	return ExprResult();
      }
    }
    bcFunc.addPushIInstr(3);
    bcFunc.addPushNativeInstr(setFuncName);
    bcFunc.addInstr(bcOpcodeCall);
    bcFunc.addInstr(bcOpcodePop);
  }

  std::vector<std::unique_ptr<CTypeRef>> params;
  params.push_back(std::move(keyType));
  params.push_back(std::move(valueType));
  std::unique_ptr<CParamTypeRef> type = std::make_unique<CParamTypeRef>(expr->loc, ctx.mapType,
									false, std::move(params));
  instantiateTypeRef(type.get(), ctx);
  return ExprResult(std::move(type));
}

static ExprResult codeGenLitIntExpr(LitIntExpr *expr, Context &ctx, BytecodeFile &bcFunc) {
  return codeGenLitInt(expr->val, expr->radix, expr->loc, ctx, bcFunc);
}

static ExprResult codeGenLitInt(const std::string &val, int radix, Location loc,
				Context &ctx, BytecodeFile &bcFunc) {
  int64_t x;
  if (!stringToInt56(val, radix, x)) {
    error(loc, "Integer literal out of bounds");
    return ExprResult();
  }
  bcFunc.addPushIInstr(x);
  return ExprResult(std::make_unique<CSimpleTypeRef>(loc, ctx.intType));
}

static ExprResult codeGenLitFloatExpr(LitFloatExpr *expr, Context &ctx, BytecodeFile &bcFunc) {
  float x;
  stringToFloat(expr->val, x);
  bcFunc.addPushFInstr(x);
  return ExprResult(std::make_unique<CSimpleTypeRef>(expr->loc, ctx.floatType));
}

static ExprResult codeGenLitBoolExpr(LitBoolExpr *expr, Context &ctx, BytecodeFile &bcFunc) {
  bcFunc.addInstr(expr->val ? bcOpcodePushTrue : bcOpcodePushFalse);
  return ExprResult(std::make_unique<CSimpleTypeRef>(expr->loc, ctx.boolType));
}

static ExprResult codeGenLitCharExpr(LitCharExpr *expr, Context &ctx, BytecodeFile &bcFunc) {
  bcFunc.addPushIInstr(expr->val[0] & 0xff);
  return ExprResult(std::make_unique<CSimpleTypeRef>(expr->loc, ctx.intType));
}

static ExprResult codeGenLitStringExpr(LitStringExpr *expr, Context &ctx, BytecodeFile &bcFunc) {
  if (!codeGenString(expr->val, expr->loc, bcFunc)) {
    return ExprResult();
  }
  return ExprResult(std::make_unique<CSimpleTypeRef>(expr->loc, ctx.stringType));
}

static ExprResult codeGenInterpStringExpr(InterpStringExpr *expr, Context &ctx,
					  BytecodeFile &bcFunc) {
  for (size_t i = 0; i < expr->parts.size(); ++i) {
    InterpStringPart *part = expr->parts[i].get();
    if (part->kind() == InterpStringPart::Kind::interpStringChars) {
      if (!codeGenInterpStringChars((InterpStringChars *)part, bcFunc)) {
	return ExprResult();
      }
    } else if (part->kind() == InterpStringPart::Kind::interpStringArg) {
      if (!codeGenInterpStringArg((InterpStringArg *)part, ctx, bcFunc)) {
	return ExprResult();
      }
    } else {
      error(part->loc, "Internal: codeGenInterpStringExpr");
      return ExprResult();
    }
    if (i > 0) {
      bcFunc.addPushIInstr(2);
      bcFunc.addPushNativeInstr(mangleStringConcatFuncName());
      bcFunc.addInstr(bcOpcodeCall);
    }
  }
  return ExprResult(std::make_unique<CSimpleTypeRef>(expr->loc, ctx.stringType));
}

static bool codeGenInterpStringChars(InterpStringChars *chars, BytecodeFile &bcFunc) {
  return codeGenString(chars->chars, chars->loc, bcFunc);
}

static bool codeGenInterpStringArg(InterpStringArg *arg, Context &ctx, BytecodeFile &bcFunc) {
  ExprResult res = codeGenExpr(arg->expr.get(), ctx, bcFunc);
  if (!res.ok) {
    return false;
  }
  if (!res.type) {
    error(arg->loc, "Non-value used in interpolated string");
    return false;
  }
  std::string formatFunc;
  if (typeCheckInt(res.type.get())) {
    formatFunc = mangleIntFormatFuncName();
  } else if (typeCheckFloat(res.type.get())) {
    formatFunc = mangleFloatFormatFuncName();
  } else if (typeCheckBool(res.type.get())) {
    formatFunc = mangleBoolFormatFuncName();
  } else if (typeCheckString(res.type.get())) {
    formatFunc = mangleStringFormatFuncName();
  } else {
    error(arg->loc, "Unsupported type for argument in interpolated string");
    return false;
  }
  bcFunc.addPushIInstr(arg->width);
  bcFunc.addPushIInstr(arg->precision);
  bcFunc.addPushIInstr(arg->format & 0xff);
  bcFunc.addPushIInstr(4);
  bcFunc.addPushNativeInstr(formatFunc);
  bcFunc.addInstr(bcOpcodeCall);
  return true;
 }

static bool codeGenString(const std::string &s, Location loc, BytecodeFile &bcFunc) {
  uint32_t label = bcFunc.allocAndSetDataLabel();
  if (s.size() > bytecodeMaxInt) {
    error(loc, "String literal too long");
    return false;
  }
  int64_t length = (int64_t)s.size();
  uint8_t tag = 0;
  bcFunc.addData(&tag, 1);
  bcFunc.addData((uint8_t *)&length, 7);
  bcFunc.addData((uint8_t *)s.c_str(), length);
  bcFunc.alignData();
  bcFunc.addPushDataInstr(label);
  return true;
}
