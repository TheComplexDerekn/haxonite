//========================================================================
//
// CodeGenFunc.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "CodeGenFunc.h"
#include "BytecodeDefs.h"
#include "CodeGenBlock.h"
#include "Error.h"
#include "Mangle.h"
#include "TypeCheck.h"
#include "TypeRefConnector.h"

static CFuncDecl *findFuncDecl(FuncDefn *func, Context &ctx);

//------------------------------------------------------------------------

bool codeGenFunc(FuncDefn *func, Context &ctx, BytecodeFile &bcFile) {
  CFuncDecl *funcDecl = findFuncDecl(func, ctx);
  if (!funcDecl) {
    error(func->loc, "Internal error: couldn't find function decl for function '%s'",
	  func->name.c_str());
    return false;
  }

  std::string mangledName = mangleFunctionName(funcDecl);
  bcFile.setFunc(mangledName);

  ctx.returnType = funcDecl->returnType.get();

  bool ok = true;

  ctx.pushFrame();
  for (std::unique_ptr<CArg> &arg : funcDecl->args) {
    if (ctx.nameExists(arg->name)) {
      error(arg->loc, "Argument '%s' duplicates an existing name", arg->name.c_str());
      ok = false;
    }
    ctx.addSymbol(std::make_unique<CArg>(arg.get()));
  }

  BytecodeFile bcFunc(bytecodeError);
  BlockResult result = codeGenBlock(func->block.get(), ctx, bcFunc);
  if (result.ok) {
    if (result.fallthrough) {
      if (ctx.returnType) {
	error(func->loc, "Function has code path(s) without a return statement");
	ok = false;
      } else {
	bcFunc.addPushIInstr(0);
	bcFunc.addInstr(bcOpcodeReturn);
      }
    }
    if (ok) {
      bcFile.appendBytecodeFile(bcFunc);
    }
  } else {
    ok = false;
  }

  ctx.popFrame();

  ctx.returnType = nullptr;

  return ok;
}

// Find the CFuncDecl corresponding to [func].
static CFuncDecl *findFuncDecl(FuncDefn *func, Context &ctx) {
  std::vector<std::unique_ptr<CTypeRef>> argTypes;
  for (std::unique_ptr<Arg> &arg : func->args) {
    std::unique_ptr<CTypeRef> argType = convertTypeRef(arg->type.get(), ctx);
    if (!argType) {
      return nullptr;
    }
    argTypes.push_back(std::move(argType));
  }

  auto range = ctx.funcs.equal_range(func->name);
  for (auto iter = range.first; iter != range.second; ++iter) {
    CFuncDecl *funcDecl = iter->second.get();
    if (functionMatch(argTypes, funcDecl)) {
      return funcDecl;
    }
  }

  return nullptr;
}
