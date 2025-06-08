//========================================================================
//
// Instantiator.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "Instantiator.h"
#include <unordered_map>
#include "Error.h"
#include "TypeCheck.h"

//------------------------------------------------------------------------

static bool instantiateStructType(CStructType *type, Context &ctx);
static bool instantiateVarStructType(CVarStructType *type, Context &ctx);
static bool instantiateSubStructType(CSubStructType *type, Context &ctx);
static bool instantiateFunc(CFuncDecl *func, Context &ctx);
static bool instantiateContainerType(CContainerType *type, Module *header,
				     CTypeRef *param1, CTypeRef *param2,
				     Location loc, Context &ctx);
static bool instantiateFuncDefn(FuncDefn *funcDefn,
				std::unordered_map<std::string, CTypeRef*> &paramMap,
				CModule *module, Context &ctx);
static std::unique_ptr<CTypeRef> instantiateTypeRef(
				     TypeRef *typeRef,
				     std::unordered_map<std::string, CTypeRef*> &paramMap,
				     Context &ctx);

//------------------------------------------------------------------------

bool instantiateContainerTypes(Context &ctx) {
  bool ok = true;

  for (auto &pair : ctx.types) {
    CType *type = pair.second.get();
    if (type->kind() == CTypeKind::structType) {
      ok &= instantiateStructType((CStructType *)type, ctx);
    } else if (type->kind() == CTypeKind::varStructType) {
      ok &= instantiateVarStructType((CVarStructType *)type, ctx);
    } else if (type->kind() == CTypeKind::subStructType) {
      ok &= instantiateSubStructType((CSubStructType *)type, ctx);
    }
  }

  // instantiating a container type adds functions, which invalidates
  // the ctx.funcs iterator --> copy to a list first, and iterate over
  // that
  std::vector<CFuncDecl*> funcs;
  for (auto &pair : ctx.funcs) {
    funcs.push_back(pair.second.get());
  }
  for (CFuncDecl *func : funcs) {
    ok &= instantiateFunc(func, ctx);
  }
  return ok;
}

static bool instantiateStructType(CStructType *type, Context &ctx) {
  bool ok = true;
  for (auto &pair : type->fields) {
    ok &= instantiateTypeRef(pair.second->type.get(), ctx);
  }
  return ok;
}

static bool instantiateVarStructType(CVarStructType *type, Context &ctx) {
  bool ok = true;
  for (auto &pair : type->fields) {
    ok &= instantiateTypeRef(pair.second->type.get(), ctx);
  }
  return ok;
}

static bool instantiateSubStructType(CSubStructType *type, Context &ctx) {
  bool ok = true;
  for (auto &pair : type->fields) {
    ok &= instantiateTypeRef(pair.second->type.get(), ctx);
  }
  return ok;
}

static bool instantiateFunc(CFuncDecl *func, Context &ctx) {
  bool ok = true;
  for (std::unique_ptr<CArg> &arg : func->args) {
    ok &= instantiateTypeRef(arg->type.get(), ctx);
  }
  if (func->returnType) {
    ok &= instantiateTypeRef(func->returnType.get(), ctx);
  }
  return ok;
}

bool instantiateTypeRef(CTypeRef *typeRef, Context &ctx) {
  if (!typeRef->isParam()) {
    return true;
  }

  CParamTypeRef *paramTypeRef = (CParamTypeRef *)typeRef;
  for (std::unique_ptr<CTypeRef> &param : paramTypeRef->params) {
    if (!instantiateTypeRef(param.get(), ctx)) {
      return false;
    }
  }

  CTypeKind typeKind = paramTypeRef->type->kind();
  CContainerType *type;
  CTypeRef *param1, *param2;
  Module *header;
  switch (typeKind) {
  case CTypeKind::vectorType:
    type = ctx.vectorType;
    param1 = paramTypeRef->params[0].get();
    param2 = nullptr;
    header = ctx.vectorHeader.get();
    break;
  case CTypeKind::setType:
    type = ctx.setType;
    param1 = paramTypeRef->params[0].get();
    param2 = nullptr;
    header = ctx.setHeader.get();
    if (!typeCheckString(param1) && !typeCheckInt(param1)) {
      error(paramTypeRef->loc, "Set element type must be String or Int");
      return false;
    }
    break;
  case CTypeKind::mapType:
    type = ctx.mapType;
    param1 = paramTypeRef->params[0].get();
    param2 = paramTypeRef->params[1].get();
    header = ctx.mapHeader.get();
    if (!typeCheckString(param1) && !typeCheckInt(param1)) {
      error(paramTypeRef->loc, "Map key type must be String or Int");
      return false;
    }
    break;
  default:
    // non-container type -- just return true
    return true;
  }

  if (type->concreteTypeExists(paramTypeRef)) {
    return true;
  }

  return instantiateContainerType(type, header, param1, param2, typeRef->loc, ctx);
}

static bool instantiateContainerType(CContainerType *type, Module *header,
				     CTypeRef *param1, CTypeRef *param2,
				     Location loc, Context &ctx) {
  if (ctx.verbose) {
    printf("Instantiating param type %s[%s", type->name.c_str(), param1->type->name.c_str());
    if (param2) {
      printf(":%s", param2->type->name.c_str());
    }
    printf("]\n");
  }

  std::unordered_map<std::string, CTypeRef*> paramMap;
  if (header->params.size() >= 1) {
    paramMap[header->params[0]] = param1;
    if (header->params.size() >= 2) {
      paramMap[header->params[1]] = param2;
    }
  }

  std::vector<std::unique_ptr<CTypeRef>> params;
  params.push_back(std::unique_ptr<CTypeRef>(param1->copy()));
  if (param2) {
    params.push_back(std::unique_ptr<CTypeRef>(param2->copy()));
  }
  type->addConcreteType(std::make_unique<CParamTypeRef>(loc, type, false, std::move(params)));

  bool ok = true;
  for (std::unique_ptr<ModuleElem> &elem : header->elems) {
    if (elem->kind() == ModuleElem::Kind::funcDefn) {
      ok &= instantiateFuncDefn((FuncDefn *)elem.get(), paramMap, type->module, ctx);
    }
  }

  return ok;
}

static bool instantiateFuncDefn(FuncDefn *funcDefn,
				std::unordered_map<std::string, CTypeRef*> &paramMap,
				CModule *module, Context &ctx) {
  bool ok = true;

  std::vector<std::unique_ptr<CArg>> cArgs;
  int argIdx = 0;
  for (std::unique_ptr<Arg> &arg : funcDefn->args) {
    std::unique_ptr<CTypeRef> cTypeRef = instantiateTypeRef(arg->type.get(), paramMap, ctx);
    if (cTypeRef) {
      cArgs.push_back(std::make_unique<CArg>(arg->loc, arg->name, std::move(cTypeRef), argIdx));
    } else {
      ok = false;
    }
    ++argIdx;
  }

  std::unique_ptr<CTypeRef> cReturnType;
  if (funcDefn->returnType) {
    cReturnType = instantiateTypeRef(funcDefn->returnType.get(), paramMap, ctx);
    if (!cReturnType) {
      ok = false;
    }
  }

  if (!ok) {
    return false;
  }
  ctx.addFunc(std::make_unique<CFuncDecl>(funcDefn->loc, funcDefn->native, true, funcDefn->name,
					  module, std::move(cArgs), std::move(cReturnType)));
  return true;
}

// Convert a TypeRef to a CTypeRef, instantiating type parameters.
static std::unique_ptr<CTypeRef> instantiateTypeRef(
				     TypeRef *typeRef,
				     std::unordered_map<std::string, CTypeRef*> &paramMap,
				     Context &ctx) {
  switch (typeRef->kind()) {
  case TypeRef::Kind::simpleTypeRef: {
    SimpleTypeRef *simpleTypeRef = (SimpleTypeRef *)typeRef;
    CType *type = ctx.findType(simpleTypeRef->name);
    if (!type) {
      error(typeRef->loc, "Undefined type '%s'", simpleTypeRef->name.c_str());
      return nullptr;
    }
    if (type->paramKind() != CParamKind::none) {
      error(typeRef->loc, "Type %s requires parameter(s)", type->name.c_str());
      return nullptr;
    }
    return std::make_unique<CSimpleTypeRef>(simpleTypeRef->loc, type);
  }
  case TypeRef::Kind::paramTypeRef: {
    ParamTypeRef *paramTypeRef = (ParamTypeRef *)typeRef;
    CType *type = ctx.findType(paramTypeRef->name);
    if (!type) {
      error(typeRef->loc, "Undefined type '%s'", paramTypeRef->name.c_str());
      return nullptr;
    }
    if (type->paramKind() == CParamKind::none) {
      error(typeRef->loc, "Type %s does not take parameter(s)", type->name.c_str());
      return nullptr;
    } else {
      int minParams = type->minParams();
      int maxParams = type->maxParams();
      if (paramTypeRef->params.size() < minParams ||
	  (maxParams >= 0 && paramTypeRef->params.size() > maxParams)) {
	error(typeRef->loc, "Incorrect number of parameters for type %s", type->name.c_str());
	return nullptr;
      }
    }
    std::vector<std::unique_ptr<CTypeRef>> params;
    for (std::unique_ptr<TypeRef> &param : paramTypeRef->params) {
      std::unique_ptr<CTypeRef> cParam = instantiateTypeRef(param.get(), paramMap, ctx);
      if (!cParam) {
	return nullptr;
      }
      params.push_back(std::move(cParam));
    }
    return std::make_unique<CParamTypeRef>(paramTypeRef->loc, type, paramTypeRef->hasReturnType,
					   std::move(params));
  }
  case TypeRef::Kind::typeVarRef: {
    TypeVarRef *typeVarRef = (TypeVarRef *)typeRef;
    auto iter = paramMap.find(typeVarRef->name);
    if (iter == paramMap.end()) {
      error(typeRef->loc, "Undefined type variable $%s", typeVarRef->name.c_str());
      return nullptr;
    }
    return std::unique_ptr<CTypeRef>(iter->second->copy());
  }
  default:
    error(typeRef->loc, "Internal (instantiateTypeRef)");
    return nullptr;
  }
}
