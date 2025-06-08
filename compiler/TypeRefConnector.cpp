//========================================================================
//
// TypeRefConnector.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "TypeRefConnector.h"
#include "Error.h"
#include "Instantiator.h"

static bool connectStructType(CStructType *type, Context &ctx);
static bool connectVarStructType(CVarStructType *type, Context &ctx);
static bool connectSubStructType(CSubStructType *type, Context &ctx);
static bool connectConst(CConst *con, Context &ctx);
static bool connectFuncDecl(CFuncDecl *func, Context &ctx);
static bool connectTypeRef(CTypeRef *typeRef, Context &ctx);
static bool connectSimpleTypeRef(CSimpleTypeRef *typeRef, Context &ctx);
static bool connectParamTypeRef(CParamTypeRef *typeRef, Context &ctx);
static std::unique_ptr<CTypeRef> convertSimpleTypeRef(SimpleTypeRef *typeRef, Context &ctx);
static std::unique_ptr<CTypeRef> convertParamTypeRef(ParamTypeRef *typeRef, Context &ctx);

//------------------------------------------------------------------------

bool connectTypeRefs(Context &ctx) {
  bool ok = true;
  for (auto &pair : ctx.types) {
    CType *type = pair.second.get();
    ctx.moduleBeingCompiled = type->module;
    if (type->kind() == CTypeKind::structType) {
      ok &= connectStructType((CStructType *)type, ctx);
    } else if (type->kind() == CTypeKind::varStructType) {
      ok &= connectVarStructType((CVarStructType *)type, ctx);
    } else if (type->kind() == CTypeKind::subStructType) {
      ok &= connectSubStructType((CSubStructType *)type, ctx);
    }
    ctx.moduleBeingCompiled = nullptr;
  }
  for (auto &pair : ctx.constants) {
    CConst *con = pair.second.get();
    ctx.moduleBeingCompiled = con->module;
    ok &= connectConst(con, ctx);
    ctx.moduleBeingCompiled = nullptr;
  }
  for (auto &pair : ctx.funcs) {
    CFuncDecl *func = pair.second.get();
    ctx.moduleBeingCompiled = func->module;
    ok &= connectFuncDecl(func, ctx);
    ctx.moduleBeingCompiled = nullptr;
  }
  return ok;
}

static bool connectStructType(CStructType *type, Context &ctx) {
  bool ok = true;
  for (auto &pair : type->fields) {
    ok &= connectTypeRef(pair.second->type.get(), ctx);
  }
  return ok;
}

static bool connectVarStructType(CVarStructType *type, Context &ctx) {
  bool ok = true;
  for (auto &pair : type->fields) {
    ok &= connectTypeRef(pair.second->type.get(), ctx);
  }
  return ok;
}

static bool connectSubStructType(CSubStructType *type, Context &ctx) {
  bool ok = true;
  for (auto &pair : type->fields) {
    ok &= connectTypeRef(pair.second->type.get(), ctx);
  }
  return ok;
}

static bool connectConst(CConst *con, Context &ctx) {
  return connectTypeRef(con->type.get(), ctx);
}

static bool connectFuncDecl(CFuncDecl *func, Context &ctx) {
  bool ok = true;
  for (std::unique_ptr<CArg> &arg : func->args) {
    ok &= connectTypeRef(arg->type.get(), ctx);
  }
  if (func->returnType) {
    ok &= connectTypeRef(func->returnType.get(), ctx);
  }
  return ok;
}

static bool connectTypeRef(CTypeRef *typeRef, Context &ctx) {
  if (typeRef->isParam()) {
    return connectParamTypeRef((CParamTypeRef *)typeRef, ctx);
  } else {
    return connectSimpleTypeRef((CSimpleTypeRef *)typeRef, ctx);
  }
}

static bool connectSimpleTypeRef(CSimpleTypeRef *typeRef, Context &ctx) {
  CType *type = ctx.findType(typeRef->name);
  if (!type) {
    error(typeRef->loc, "Undefined type '%s'", typeRef->name.c_str());
    return false;
  }
  if (type->paramKind() != CParamKind::none) {
    error(typeRef->loc, "Type %s requires parameter(s)", type->name.c_str());
    return false;
  }
  typeRef->name = "";
  typeRef->type = type;
  return true;
}

static bool connectParamTypeRef(CParamTypeRef *typeRef, Context &ctx) {
  CType *type = ctx.findType(typeRef->name);
  if (!type) {
    error(typeRef->loc, "Undefined type '%s'", typeRef->name.c_str());
    return false;
  }
  bool ok = true;
  if (type->paramKind() == CParamKind::none) {
    error(typeRef->loc, "Type %s does not take parameter(s)", type->name.c_str());
    ok = false;
  } else {
    int minParams = type->minParams();
    int maxParams = type->maxParams();
    if (typeRef->params.size() < minParams ||
	(maxParams >= 0 && typeRef->params.size() > maxParams)) {
      error(typeRef->loc, "Incorrect number of parameters for type %s", type->name.c_str());
      ok = false;
    }
  }
  for (std::unique_ptr<CTypeRef> &param : typeRef->params) {
    ok &= connectTypeRef(param.get(), ctx);
  }
  if (ok) {
    typeRef->name = "";
    typeRef->type = type;
  }
  return ok;
}

std::unique_ptr<CTypeRef> convertTypeRef(TypeRef *typeRef, Context &ctx) {
  switch (typeRef->kind()) {
  case TypeRef::Kind::simpleTypeRef:
    return convertSimpleTypeRef((SimpleTypeRef *)typeRef, ctx);
  case TypeRef::Kind::paramTypeRef:
    return convertParamTypeRef((ParamTypeRef *)typeRef, ctx);
  default:
    error(typeRef->loc, "Internal (convertTypeRef)");
    return nullptr;
  }
}

static std::unique_ptr<CTypeRef> convertSimpleTypeRef(SimpleTypeRef *typeRef, Context &ctx) {
  CType *type = ctx.findType(typeRef->name);
  if (!type) {
    error(typeRef->loc, "Undefined type '%s'", typeRef->name.c_str());
    return nullptr;
  }
  if (type->paramKind() != CParamKind::none) {
    error(typeRef->loc, "Type %s requires parameter(s)", type->name.c_str());
    return nullptr;
  }
  return std::make_unique<CSimpleTypeRef>(typeRef->loc, type);
}

static std::unique_ptr<CTypeRef> convertParamTypeRef(ParamTypeRef *typeRef, Context &ctx) {
  CType *type = ctx.findType(typeRef->name);
  if (!type) {
    error(typeRef->loc, "Undefined type '%s'", typeRef->name.c_str());
    return nullptr;
  }
  if (type->paramKind() == CParamKind::none) {
    error(typeRef->loc, "Type %s does not take parameter(s)", type->name.c_str());
    return nullptr;
  } else {
    int minParams = type->minParams();
    int maxParams = type->maxParams();
    if (typeRef->params.size() < minParams ||
	(maxParams >= 0 && typeRef->params.size() > maxParams)) {
      error(typeRef->loc, "Incorrect number of parameters for type %s", type->name.c_str());
      return nullptr;
    }
  }

  std::vector<std::unique_ptr<CTypeRef>> params;
  for (std::unique_ptr<TypeRef> &param : typeRef->params) {
    std::unique_ptr<CTypeRef> cParam = convertTypeRef(param.get(), ctx);
    if (!cParam) {
      return nullptr;
    }
    params.push_back(std::move(cParam));
  }
  std::unique_ptr<CParamTypeRef> cTypeRef = std::make_unique<CParamTypeRef>(typeRef->loc, type,
									    typeRef->hasReturnType,
									    std::move(params));

  if (!instantiateTypeRef(cTypeRef.get(), ctx)) {
    return nullptr;
  }

  return cTypeRef;
}
