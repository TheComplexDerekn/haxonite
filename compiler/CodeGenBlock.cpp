//========================================================================
//
// CodeGenBlock.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "CodeGenBlock.h"
#include <set>
#include "BytecodeDefs.h"
#include "CodeGenExpr.h"
#include "Error.h"
#include "Mangle.h"
#include "TypeCheck.h"
#include "TypeRefConnector.h"

//------------------------------------------------------------------------

static BlockResult codeGenStmt(Stmt *stmt, Context &ctx, BytecodeFile &bcFunc);
static BlockResult codeGenVarStmt(VarStmt *stmt, Context &ctx, BytecodeFile &bcFunc);
static BlockResult codeGenIfStmt(IfStmt *stmt, Context &ctx, BytecodeFile &bcFunc);
static BlockResult codeGenWhileStmt(WhileStmt *stmt, Context &ctx, BytecodeFile &bcFunc);
static BlockResult codeGenForStmt(ForStmt *stmt, Context &ctx, BytecodeFile &bcFunc);
static BlockResult codeGenForRangeStmt(ForStmt *stmt, Context &ctx, BytecodeFile &bcFunc);
static BlockResult codeGenForContainerStmt(ForStmt *stmt, Context &ctx, BytecodeFile &bcFunc);
static BlockResult codeGenBreakStmt(BreakStmt *stmt, Context &ctx, BytecodeFile &bcFunc);
static BlockResult codeGenContinueStmt(ContinueStmt *stmt, Context &ctx, BytecodeFile &bcFunc);
static BlockResult codeGenTypematchStmt(TypematchStmt *stmt, Context &ctx, BytecodeFile &bcFunc);
static BlockResult codeGenReturnStmt(ReturnStmt *stmt, Context &ctx, BytecodeFile &bcFunc);
static BlockResult codeGenAssignStmt(AssignStmt *stmt, Context &ctx, BytecodeFile &bcFunc);
static BlockResult codeGenAssignIdentStmt(IdentExpr *lhs, BytecodeFile &bcRHS, ExprResult rhsRes,
					  Context &ctx, BytecodeFile &bcFunc);
static BlockResult codeGenAssignMemberStmt(MemberExpr *lhs, BytecodeFile &bcRHS, ExprResult rhsRes,
					   Context &ctx, BytecodeFile &bcFunc);
static BlockResult codeGenAssignIndexStmt(IndexExpr *lhs, BytecodeFile &bcRHS, ExprResult rhsRes,
					  Context &ctx, BytecodeFile &bcFunc);
static BlockResult codeGenExprStmt(ExprStmt *stmt, Context &ctx, BytecodeFile &bcFunc);

//------------------------------------------------------------------------

BlockResult codeGenBlock(Block *block, Context &ctx, BytecodeFile &bcFunc) {
  ctx.pushFrame();

  bool ok = true;
  bool fallthrough = true;
  for (std::unique_ptr<Stmt> &stmt : block->stmts) {
    BlockResult res = codeGenStmt(stmt.get(), ctx, bcFunc);
    ok &= res.ok;
    fallthrough = res.fallthrough;
  }

  int frameDelta = ctx.frameSize();
  ctx.popFrame();
  frameDelta -= ctx.frameSize();
  for (int i = 0; i < frameDelta; ++i) {
    bcFunc.addInstr(bcOpcodePop);
  }

  if (!ok) {
    return BlockResult();
  }
  return BlockResult(fallthrough);
}

static BlockResult codeGenStmt(Stmt *stmt, Context &ctx, BytecodeFile &bcFunc) {
  switch (stmt->kind()) {
  case Stmt::Kind::varStmt:
    return codeGenVarStmt((VarStmt *)stmt, ctx, bcFunc);
  case Stmt::Kind::ifStmt:
    return codeGenIfStmt((IfStmt *)stmt, ctx, bcFunc);
  case Stmt::Kind::whileStmt:
    return codeGenWhileStmt((WhileStmt *)stmt, ctx, bcFunc);
  case Stmt::Kind::forStmt:
    return codeGenForStmt((ForStmt *)stmt, ctx, bcFunc);
  case Stmt::Kind::breakStmt:
    return codeGenBreakStmt((BreakStmt *)stmt, ctx, bcFunc);
  case Stmt::Kind::continueStmt:
    return codeGenContinueStmt((ContinueStmt *)stmt, ctx, bcFunc);
  case Stmt::Kind::typematchStmt:
    return codeGenTypematchStmt((TypematchStmt *)stmt, ctx, bcFunc);
  case Stmt::Kind::returnStmt:
    return codeGenReturnStmt((ReturnStmt *)stmt, ctx, bcFunc);
  case Stmt::Kind::assignStmt:
    return codeGenAssignStmt((AssignStmt *)stmt, ctx, bcFunc);
  case Stmt::Kind::exprStmt:
    return codeGenExprStmt((ExprStmt *)stmt, ctx, bcFunc);
  default:
    error(stmt->loc, "Internal error in codeGenStmt()");
    return BlockResult();
  }
}

static BlockResult codeGenVarStmt(VarStmt *stmt, Context &ctx, BytecodeFile &bcFunc) {
  if (ctx.nameExists(stmt->name)) {
    error(stmt->loc, "Local variable '%s' duplicates an existing name", stmt->name.c_str());
    return BlockResult();
  }

  ExprResult res = codeGenExpr(stmt->expr.get(), ctx, bcFunc);
  if (!res.ok) {
    return BlockResult();
  }
  if (!res.type) {
    error(stmt->loc, "Local variable initializer doesn't have a value");
    return BlockResult();
  }

  ctx.incFrameSize();
  ctx.addSymbol(std::make_unique<CVar>(stmt->loc, stmt->name, std::move(res.type),
				       ctx.frameSize(), true));

  return BlockResult(true);
}

static BlockResult codeGenIfStmt(IfStmt *stmt, Context &ctx, BytecodeFile &bcFunc) {
  uint32_t endLabel = bcFunc.allocCodeLabel();

  bool fallthrough = false;

  for (size_t i = 0; i < stmt->tests.size(); ++i) {
    ExprResult testRes = codeGenExpr(stmt->tests[i].get(), ctx, bcFunc);
    if (!testRes.ok) {
      return BlockResult();
    }
    if (!testRes.type) {
      error(stmt->loc, "If/then statement test doesn't have a value");
      return BlockResult();
    }
    if (!typeCheckBool(testRes.type.get())) {
      error(stmt->loc, "If/then statement test isn't a boolean");
      return BlockResult();
    }

    uint32_t nextLabel = bcFunc.allocCodeLabel();
    bcFunc.addBranchInstr(bcOpcodeBranchFalse, nextLabel);

    BlockResult blockRes = codeGenBlock(stmt->blocks[i].get(), ctx, bcFunc);
    if (!blockRes.ok) {
      return BlockResult();
    }
    fallthrough |= blockRes.fallthrough;

    bcFunc.addBranchInstr(bcOpcodeBranch, endLabel);
    bcFunc.setCodeLabel(nextLabel);
  }

  if (stmt->elseBlock) {
    BlockResult blockRes = codeGenBlock(stmt->elseBlock.get(), ctx, bcFunc);
    if (!blockRes.ok) {
      return BlockResult();
    }
    fallthrough |= blockRes.fallthrough;
  } else {
    fallthrough = true;
  }

  bcFunc.setCodeLabel(endLabel);

  return BlockResult(fallthrough);
}

static BlockResult codeGenWhileStmt(WhileStmt *stmt, Context &ctx, BytecodeFile &bcFunc) {
  uint32_t continueLabel = bcFunc.allocCodeLabel();
  uint32_t breakLabel = bcFunc.allocCodeLabel();

  bcFunc.setCodeLabel(continueLabel);

  ExprResult testRes = codeGenExpr(stmt->test.get(), ctx, bcFunc);
  if (!testRes.ok) {
    return BlockResult();
  }
  if (!testRes.type) {
    error(stmt->loc, "While statement test doesn't have a value");
    return BlockResult();
  }
  if (!typeCheckBool(testRes.type.get())) {
    error(stmt->loc, "While statement test isn't a boolean");
    return BlockResult();
  }

  bcFunc.addBranchInstr(bcOpcodeBranchFalse, breakLabel);

  ctx.enterLoop(continueLabel, breakLabel);
  BlockResult blockRes = codeGenBlock(stmt->block.get(), ctx, bcFunc);
  if (!blockRes.ok) {
    return BlockResult();
  }
  ctx.exitLoop();

  bcFunc.addBranchInstr(bcOpcodeBranch, continueLabel);

  bcFunc.setCodeLabel(breakLabel);

  return BlockResult(true);
}

static BlockResult codeGenForStmt(ForStmt *stmt, Context &ctx, BytecodeFile &bcFunc) {
  if (stmt->expr2) {
    return codeGenForRangeStmt(stmt, ctx, bcFunc);
  } else {
    return codeGenForContainerStmt(stmt, ctx, bcFunc);
  }
}

static BlockResult codeGenForRangeStmt(ForStmt *stmt, Context &ctx, BytecodeFile &bcFunc) {
  if (ctx.nameExists(stmt->var)) {
    error(stmt->loc, "For-loop index variable '%s' duplicates an existing name", stmt->var.c_str());
    return BlockResult();
  }

  uint32_t topLabel = bcFunc.allocCodeLabel();
  uint32_t continueLabel = bcFunc.allocCodeLabel();
  uint32_t breakLabel = bcFunc.allocCodeLabel();

  ctx.pushFrame();

  //--- range start
  ExprResult res1 = codeGenExpr(stmt->expr1.get(), ctx, bcFunc);
  if (!res1.ok) {
    ctx.popFrame();
    return BlockResult();
  }
  if (!res1.type) {
    error(stmt->loc, "For loop range start doesn't have a value");
    ctx.popFrame();
    return BlockResult();
  }
  if (!typeCheckInt(res1.type.get())) {
    error(stmt->loc, "For loop range start must be an Int");
    ctx.popFrame();
    return BlockResult();
  }

  //--- index variable
  ctx.incFrameSize();
  int loopVarIdx = ctx.frameSize();
  ctx.addSymbol(std::make_unique<CVar>(stmt->loc, stmt->var, std::move(res1.type),
				       loopVarIdx, false));

  //--- range end
  ExprResult res2 = codeGenExpr(stmt->expr2.get(), ctx, bcFunc);
  if (!res2.ok) {
    ctx.popFrame();
    return BlockResult();
  }
  if (!res2.type) {
    error(stmt->loc, "For loop range end doesn't have a value");
    ctx.popFrame();
    return BlockResult();
  }
  if (!typeCheckInt(res2.type.get())) {
    error(stmt->loc, "For loop range end must be an Int");
    ctx.popFrame();
    return BlockResult();
  }
  ctx.incFrameSize();
  int rangeEndIdx = ctx.frameSize();

  //--- test
  bcFunc.setCodeLabel(topLabel);
  bcFunc.addPushIInstr(loopVarIdx);
  bcFunc.addInstr(bcOpcodeGetVar);
  bcFunc.addPushIInstr(rangeEndIdx);
  bcFunc.addInstr(bcOpcodeGetVar);
  bcFunc.addInstr(bcOpcodeCmple);
  bcFunc.addBranchInstr(bcOpcodeBranchFalse, breakLabel);

  //--- body
  ctx.enterLoop(continueLabel, breakLabel);
  BlockResult blockRes = codeGenBlock(stmt->block.get(), ctx, bcFunc);
  if (!blockRes.ok) {
    ctx.popFrame();
    return BlockResult();
  }
  ctx.exitLoop();

  //--- increment and loop
  bcFunc.setCodeLabel(continueLabel);
  bcFunc.addPushIInstr(loopVarIdx);
  bcFunc.addInstr(bcOpcodeGetVar);
  bcFunc.addPushIInstr(1);
  bcFunc.addInstr(bcOpcodeAdd);
  bcFunc.addPushIInstr(loopVarIdx);
  bcFunc.addInstr(bcOpcodePutVar);
  bcFunc.addBranchInstr(bcOpcodeBranch, topLabel);

  //--- end of loop
  bcFunc.setCodeLabel(breakLabel);
  ctx.popFrame();
  bcFunc.addInstr(bcOpcodePop);  // rangeEnd
  bcFunc.addInstr(bcOpcodePop);  // loopVar

  return BlockResult(true);
}

static BlockResult codeGenForContainerStmt(ForStmt *stmt, Context &ctx, BytecodeFile &bcFunc) {
  if (ctx.nameExists(stmt->var)) {
    error(stmt->loc, "For-loop index variable '%s' duplicates an existing name", stmt->var.c_str());
    return BlockResult();
  }

  uint32_t topLabel = bcFunc.allocCodeLabel();
  uint32_t continueLabel = bcFunc.allocCodeLabel();
  uint32_t breakLabel = bcFunc.allocCodeLabel();

  ctx.pushFrame();

  //--- container
  ExprResult res = codeGenExpr(stmt->expr1.get(), ctx, bcFunc);
  if (!res.ok) {
    ctx.popFrame();
    return BlockResult();
  }
  if (!res.type) {
    error(stmt->loc, "For loop container doesn't have a value");
    ctx.popFrame();
    return BlockResult();
  }
  ctx.incFrameSize();
  int containerIdx = ctx.frameSize();

  //--- check container type
  std::unique_ptr<CTypeRef> elemType;
  std::string ifirstFuncName;
  std::string imoreFuncName;
  std::string inextFuncName;
  std::string igetFuncName;
  switch (res.type->type->kind()) {
  case CTypeKind::vectorType:
    elemType = std::unique_ptr<CTypeRef>(((CParamTypeRef *)res.type.get())->params[0]->copy());
    ifirstFuncName = mangleVectorIfirstFuncName();
    imoreFuncName = mangleVectorImoreFuncName();
    inextFuncName = mangleVectorInextFuncName();
    igetFuncName = mangleVectorIgetFuncName();
    break;
  case CTypeKind::setType:
    elemType = std::unique_ptr<CTypeRef>(((CParamTypeRef *)res.type.get())->params[0]->copy());
    if (typeCheckString(elemType.get()) ||
	typeCheckInt(elemType.get())) {
      ifirstFuncName = mangleSetIfirstFuncName(elemType.get());
      imoreFuncName = mangleSetImoreFuncName(elemType.get());
      inextFuncName = mangleSetInextFuncName(elemType.get());
      igetFuncName = mangleSetIgetFuncName(elemType.get());
    } else {
      error(stmt->loc, "Internal: bad Set param (codeGenForContainerStmt)");
      ctx.popFrame();
      return BlockResult();
    }
    break;
  case CTypeKind::mapType:
    elemType = std::unique_ptr<CTypeRef>(((CParamTypeRef *)res.type.get())->params[0]->copy());
    if (typeCheckString(elemType.get()) ||
	typeCheckInt(elemType.get())) {
      ifirstFuncName = mangleMapIfirstFuncName(elemType.get());
      imoreFuncName = mangleMapImoreFuncName(elemType.get());
      inextFuncName = mangleMapInextFuncName(elemType.get());
      igetFuncName = mangleMapIgetFuncName(elemType.get());
    } else {
      error(stmt->loc, "Internal: bad Map param (codeGenForContainerStmt)");
      ctx.popFrame();
      return BlockResult();
    }
    break;
  default:
    error(stmt->loc, "For loop container must be Vector, Set, or Map");
    ctx.popFrame();
    return BlockResult();
  }

  //--- call ifirst
  bcFunc.addPushIInstr(containerIdx);
  bcFunc.addInstr(bcOpcodeGetVar);
  bcFunc.addPushIInstr(1);
  bcFunc.addPushNativeInstr(ifirstFuncName);
  bcFunc.addInstr(bcOpcodeCall);
  ctx.incFrameSize();
  int iterIdx = ctx.frameSize();

  //--- test: call imore
  bcFunc.setCodeLabel(topLabel);
  bcFunc.addPushIInstr(containerIdx);
  bcFunc.addInstr(bcOpcodeGetVar);
  bcFunc.addPushIInstr(iterIdx);
  bcFunc.addInstr(bcOpcodeGetVar);
  bcFunc.addPushIInstr(2);
  bcFunc.addPushNativeInstr(imoreFuncName);
  bcFunc.addInstr(bcOpcodeCall);
  bcFunc.addBranchInstr(bcOpcodeBranchFalse, breakLabel);

  //--- call iget
  bcFunc.addPushIInstr(containerIdx);
  bcFunc.addInstr(bcOpcodeGetVar);
  bcFunc.addPushIInstr(iterIdx);
  bcFunc.addInstr(bcOpcodeGetVar);
  bcFunc.addPushIInstr(2);
  bcFunc.addPushNativeInstr(igetFuncName);
  bcFunc.addInstr(bcOpcodeCall);
  ctx.incFrameSize();
  int loopVarIdx = ctx.frameSize();
  ctx.addSymbol(std::make_unique<CVar>(stmt->loc, stmt->var, std::move(elemType),
				       loopVarIdx, false));

  //--- body
  ctx.enterLoop(continueLabel, breakLabel);
  BlockResult blockRes = codeGenBlock(stmt->block.get(), ctx, bcFunc);
  if (!blockRes.ok) {
    ctx.popFrame();
    return BlockResult();
  }
  ctx.exitLoop();

  //--- pop the element (loop var)
  bcFunc.setCodeLabel(continueLabel);
  bcFunc.addInstr(bcOpcodePop);

  //--- call inext and loop
  bcFunc.addPushIInstr(containerIdx);
  bcFunc.addInstr(bcOpcodeGetVar);
  bcFunc.addPushIInstr(iterIdx);
  bcFunc.addInstr(bcOpcodeGetVar);
  bcFunc.addPushIInstr(2);
  bcFunc.addPushNativeInstr(inextFuncName);
  bcFunc.addInstr(bcOpcodeCall);
  bcFunc.addPushIInstr(iterIdx);
  bcFunc.addInstr(bcOpcodePutVar);
  bcFunc.addBranchInstr(bcOpcodeBranch, topLabel);

  //--- end of loop
  bcFunc.setCodeLabel(breakLabel);
  ctx.popFrame();
  bcFunc.addInstr(bcOpcodePop);  // iter
  bcFunc.addInstr(bcOpcodePop);  // container

  return BlockResult(true);
}

static BlockResult codeGenBreakStmt(BreakStmt *stmt, Context &ctx, BytecodeFile &bcFunc) {
  Frame *loopFrame = ctx.findLoop();
  if (!loopFrame) {
    error(stmt->loc, "Break statement is not inside a loop");
    return BlockResult();
  }

  int frameDelta = ctx.frameSize() - loopFrame->frameSize;
  for (int i = 0; i < frameDelta; ++i) {
    bcFunc.addInstr(bcOpcodePop);
  }

  bcFunc.addBranchInstr(bcOpcodeBranch, loopFrame->breakLabel);
  return BlockResult(false);
}

static BlockResult codeGenContinueStmt(ContinueStmt *stmt, Context &ctx, BytecodeFile &bcFunc) {
  Frame *loopFrame = ctx.findLoop();
  if (!loopFrame) {
    error(stmt->loc, "Continue statement is not inside a loop");
    return BlockResult();
  }

  int frameDelta = ctx.frameSize() - loopFrame->frameSize;
  for (int i = 0; i < frameDelta; ++i) {
    bcFunc.addInstr(bcOpcodePop);
  }

  bcFunc.addBranchInstr(bcOpcodeBranch, loopFrame->continueLabel);
  return BlockResult(false);
}

static BlockResult codeGenTypematchStmt(TypematchStmt *stmt, Context &ctx, BytecodeFile &bcFunc) {
  //--- evaluate the expression
  ExprResult res = codeGenExpr(stmt->expr.get(), ctx, bcFunc);
  if (!res.ok) {
    return BlockResult();
  }
  if (!res.type) {
    error(stmt->loc, "Typematch statement expression doesn't have a value");
    return BlockResult();
  }
  if (!typeCheckVarStruct(res.type.get())) {
    error(stmt->loc, "Typematch statement expression isn't a varstruct");
    return BlockResult();
  }
  CVarStructType *varType = (CVarStructType *)res.type->type;

  //--- read the ID field
  bcFunc.addGetStackInstr(0);
  bcFunc.addPushIInstr(0);
  bcFunc.addInstr(bcOpcodeLoad);

  //--- handle the cases
  uint32_t endLabel = bcFunc.allocCodeLabel();
  bool fallthrough = false;
  std::set<int> handledCases;
  TypematchCase *defaultCase = nullptr;
  bool ok = true;
  for (std::unique_ptr<TypematchCase> &c : stmt->cases) {
    if (!c->type) {
      if (defaultCase) {
	error(c->loc, "Duplicate default case in typematch");
	ok = false;
      } else {
	defaultCase = c.get();
      }
      continue;
    }

    //--- substruct case
    std::unique_ptr<CTypeRef> cType = convertTypeRef(c->type.get(), ctx);
    if (!cType) {
      ok = false;
      continue;
    }
    if (!typeCheckSubStruct(cType.get())) {
      error(c->loc, "Typematch case isn't a substruct");
      ok = false;
      continue;
    }
    CSubStructType *subType = (CSubStructType *)cType->type;
    if (subType->parent != varType) {
      error(c->loc, "Typematch case is not a substruct of %s", varType->name.c_str());
      ok = false;
      continue;
    }
    if (handledCases.count(subType->id)) {
      error(c->loc, "Duplicate case in typematch");
      ok = false;
      continue;
    }
    if (ctx.nameExists(c->var)) {
      error(c->loc, "Typematch case variable '%s' duplicates an existing name", c->var.c_str());
      ok = false;
      continue;
    }
    handledCases.insert(subType->id);
    bcFunc.addGetStackInstr(0);
    bcFunc.addPushIInstr(subType->id);
    bcFunc.addInstr(bcOpcodeCmpeq);
    uint32_t nextLabel = bcFunc.allocCodeLabel();
    bcFunc.addBranchInstr(bcOpcodeBranchFalse, nextLabel);
    ctx.pushFrame();
    ctx.incFrameSize();
    int varIdx = ctx.frameSize();
    ctx.incFrameSize();
    ctx.addSymbol(std::make_unique<CVar>(c->loc, c->var,
					 std::unique_ptr<CTypeRef>(cType->copy()),
					 varIdx, false));
    BlockResult blockRes = codeGenBlock(c->block.get(), ctx, bcFunc);
    if (!blockRes.ok) {
      ok = false;
    }
    fallthrough |= blockRes.fallthrough;
    bcFunc.addBranchInstr(bcOpcodeBranch, endLabel);
    bcFunc.setCodeLabel(nextLabel);
    ctx.popFrame();
  }

  //--- default case
  if (defaultCase) {
    ctx.pushFrame();
    ctx.incFrameSize();
    ctx.incFrameSize();
    BlockResult blockRes = codeGenBlock(defaultCase->block.get(), ctx, bcFunc);
    if (!blockRes.ok) {
      ok = false;
    }
    fallthrough |= blockRes.fallthrough;
    ctx.popFrame();
  }

  bcFunc.setCodeLabel(endLabel);
  bcFunc.addInstr(bcOpcodePop);  // ID field
  bcFunc.addInstr(bcOpcodePop);  // expression

  if (!ok) {
    return BlockResult();
  }

  if (!defaultCase && handledCases.size() != varType->subStructs.size()) {
    error(stmt->loc, "Unhandled substruct type in typematch");
    return BlockResult();
  }

  return BlockResult(fallthrough);
}

static BlockResult codeGenReturnStmt(ReturnStmt *stmt, Context &ctx, BytecodeFile &bcFunc) {
  if (stmt->expr) {
    ExprResult res = codeGenExpr(stmt->expr.get(), ctx, bcFunc);
    if (!res.ok) {
      return BlockResult();
    }
    if (!res.type) {
      error(stmt->loc, "Non-value in return statement");
      return BlockResult();
    }
    if (!ctx.returnType) {
      error(stmt->loc, "Return statement with value in function without return type");
      return BlockResult();
    }
    if (!typeMatch(res.type.get(), ctx.returnType)) {
      error(stmt->loc, "Type mismatch in return statement");
      return BlockResult();
    }
  } else {
    if (ctx.returnType) {
      error(stmt->loc, "Return statement without value in function with return type");
      return BlockResult();
    }
    bcFunc.addPushIInstr(0);
  }
  bcFunc.addInstr(bcOpcodeReturn);
  return BlockResult(false);
}

static BlockResult codeGenAssignStmt(AssignStmt *stmt, Context &ctx, BytecodeFile &bcFunc) {
  BytecodeFile bcRHS(bytecodeError);
  ExprResult rhsRes = codeGenExpr(stmt->rhs.get(), ctx, bcRHS);
  if (!rhsRes.ok) {
    return BlockResult();
  }
  if (!rhsRes.type) {
    error(stmt->rhs->loc, "Non-value on right side of assignment");
    return BlockResult();
  }

  if (stmt->lhs->kind() == Expr::Kind::identExpr) {
    return codeGenAssignIdentStmt((IdentExpr *)stmt->lhs.get(), bcRHS, std::move(rhsRes),
				  ctx, bcFunc);

  } else if (stmt->lhs->kind() == Expr::Kind::memberExpr) {
    return codeGenAssignMemberStmt((MemberExpr *)stmt->lhs.get(), bcRHS, std::move(rhsRes),
				   ctx, bcFunc);

  } else if (stmt->lhs->kind() == Expr::Kind::indexExpr) {
    return codeGenAssignIndexStmt((IndexExpr *)stmt->lhs.get(), bcRHS, std::move(rhsRes),
				  ctx, bcFunc);

  } else {
    error(stmt->loc, "Invalid expression on left side of assignment");
    return BlockResult();
  }
}

static BlockResult codeGenAssignIdentStmt(IdentExpr *lhs, BytecodeFile &bcRHS, ExprResult rhsRes,
					  Context &ctx, BytecodeFile &bcFunc) {
  CSymbol *sym = ctx.findSymbol(lhs->name);
  if (!sym) {
    error(lhs->loc, "Undefined symbol '%s'", lhs->name.c_str());
    return BlockResult();
  }
  if (!sym->isWritable()) {
    error(lhs->loc, "Symbol '%s' is not writable", lhs->name.c_str());
    return BlockResult();
  }
  if (!typeMatch(rhsRes.type.get(), sym->type.get())) {
    error(lhs->loc, "Type mismatch in assignment");
    return BlockResult();
  }
  bcFunc.appendBytecodeFile(bcRHS);
  bcFunc.addPushIInstr(((CVar *)sym)->frameIdx);
  bcFunc.addInstr(bcOpcodePutVar);
  return BlockResult(true);
}

static BlockResult codeGenAssignMemberStmt(MemberExpr *lhs, BytecodeFile &bcRHS, ExprResult rhsRes,
					   Context &ctx, BytecodeFile &bcFunc) {
  bcFunc.appendBytecodeFile(bcRHS);

  ExprResult objRes = codeGenExpr(lhs->lhs.get(), ctx, bcFunc);
  if (!objRes.ok) {
    return BlockResult();
  }
  if (!objRes.type) {
    error(lhs->lhs->loc, "Non-value on left side of member assignment");
    return BlockResult();
  }

  CField *field = findField(objRes.type.get(), lhs->member, lhs->loc, "member assignment");
  if (!field) {
    return BlockResult();
  }

  if (!typeMatch(rhsRes.type.get(), field->type.get())) {
    error(lhs->loc, "Type mismatch in assignment");
    return BlockResult();
  }

  bcFunc.addPushIInstr(field->fieldIdx);
  bcFunc.addInstr(bcOpcodeStore);
  return BlockResult(true);
}

static BlockResult codeGenAssignIndexStmt(IndexExpr *lhs, BytecodeFile &bcRHS, ExprResult rhsRes,
					  Context &ctx, BytecodeFile &bcFunc) {
  ExprResult objRes = codeGenExpr(lhs->obj.get(), ctx, bcFunc);
  if (!objRes.ok) {
    return BlockResult();
  }
  if (!objRes.type) {
    error(lhs->obj->loc, "Non-value as object in index assignment");
    return BlockResult();
  }
  if (!(typeCheckVector(objRes.type.get()) ||
	typeCheckMap(objRes.type.get()))) {
    error(lhs->obj->loc, "Invalid type for object in index assignment");
    return BlockResult();
  }

  ExprResult idxRes = codeGenExpr(lhs->idx.get(), ctx, bcFunc);
  if (!idxRes.ok) {
    return BlockResult();
  }
  if (!idxRes.type) {
    error(lhs->idx->loc, "Non-value as index in index assignment");
    return BlockResult();
  }

  std::vector<ExprResult> args;
  args.push_back(ExprResult(std::unique_ptr<CTypeRef>(objRes.type->copy())));
  args.push_back(ExprResult(std::unique_ptr<CTypeRef>(idxRes.type->copy())));
  args.push_back(ExprResult(std::unique_ptr<CTypeRef>(rhsRes.type->copy())));
  
  CFuncDecl *funcDecl = findFunction("set", args, ctx);
  if (!funcDecl) {
    error(lhs->idx->loc, "Invalid type for index or value in index assignment");
    return BlockResult();
  }

  bcFunc.appendBytecodeFile(bcRHS);
  bcFunc.addPushIInstr(3);
  bcFunc.addPushNativeInstr(mangleFunctionName(funcDecl));
  bcFunc.addInstr(bcOpcodeCall);
  bcFunc.addInstr(bcOpcodePop);

  return BlockResult(true);
}

static BlockResult codeGenExprStmt(ExprStmt *stmt, Context &ctx, BytecodeFile &bcFunc) {
  ExprResult res = codeGenExpr(stmt->expr.get(), ctx, bcFunc);
  if (!res.ok) {
    return BlockResult();
  }
  if (res.type) {
    bcFunc.addInstr(bcOpcodePop);
  }
  return BlockResult(true);
}
