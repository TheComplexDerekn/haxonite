//========================================================================
//
// TypeCheck.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "TypeCheck.h"

//------------------------------------------------------------------------

bool functionCollision(CFuncDecl *funcDecl1, CFuncDecl *funcDecl2) {
  if (funcDecl1->args.size() != funcDecl2->args.size()) {
    return false;
  }
  for (size_t i = 0; i < funcDecl1->args.size(); ++i) {
    if (!typeMatch(funcDecl1->args[i]->type.get(), funcDecl2->args[i]->type.get())) {
      return false;
    }
  }
  return true;
}

bool functionMatch(std::vector<std::unique_ptr<CTypeRef>> &argTypes, CFuncDecl *funcDecl) {
  if (argTypes.size() != funcDecl->args.size()) {
    return false;
  }
  for (size_t i = 0; i < argTypes.size(); ++i) {
    if (!typeMatch(argTypes[i].get(), funcDecl->args[i]->type.get())) {
      return false;
    }
  }
  return true;
}

bool functionMatch(std::vector<ExprResult> &argResults, CFuncDecl *funcDecl) {
  if (argResults.size() != funcDecl->args.size()) {
    return false;
  }
  for (size_t i = 0; i < argResults.size(); ++i) {
    if (!typeMatch(argResults[i].type.get(), funcDecl->args[i]->type.get())) {
      return false;
    }
  }
  return true;
}

bool typeMatch(CTypeRef *typeRef1, CTypeRef *typeRef2) {
  if (typeRef1->isParam() != typeRef2->isParam() ||
      typeRef1->type != typeRef2->type) {
    return false;
  }
  if (typeRef1->isParam()) {
    CParamTypeRef *paramTypeRef1 = (CParamTypeRef *)typeRef1;
    CParamTypeRef *paramTypeRef2 = (CParamTypeRef *)typeRef2;
    if (paramTypeRef1->params.size() != paramTypeRef2->params.size() ||
	paramTypeRef1->hasReturnType != paramTypeRef2->hasReturnType) {
      return false;
    }
    for (size_t i = 0; i < paramTypeRef1->params.size(); ++i) {
      if (!typeMatch(paramTypeRef1->params[i].get(), paramTypeRef2->params[i].get())) {
	return false;
      }
    }
  }
  return true;
}

bool typeCheckInt(CTypeRef *typeRef) {
  return typeRef->type->kind() == CTypeKind::intType;
}

bool typeCheckBool(CTypeRef *typeRef) {
  return typeRef->type->kind() == CTypeKind::boolType;
}

bool typeCheckFloat(CTypeRef *typeRef) {
  return typeRef->type->kind() == CTypeKind::floatType;
}

bool typeCheckString(CTypeRef *typeRef) {
  return typeRef->type->kind() == CTypeKind::stringType;
}

bool typeCheckStringBuf(CTypeRef *typeRef) {
  return typeRef->type->kind() == CTypeKind::stringBufType;
}

bool typeCheckVector(CTypeRef *typeRef) {
  return typeRef->type->kind() == CTypeKind::vectorType;
}

bool typeCheckSet(CTypeRef *typeRef) {
  return typeRef->type->kind() == CTypeKind::setType;
}

bool typeCheckMap(CTypeRef *typeRef) {
  return typeRef->type->kind() == CTypeKind::mapType;
}

bool typeCheckFuncPointer(CTypeRef *typeRef) {
  return typeRef->type->kind() == CTypeKind::funcType;
}

bool typeCheckResult(CTypeRef *typeRef) {
  return typeRef->type->kind() == CTypeKind::resultType;
}

bool typeCheckStruct(CTypeRef *typeRef) {
  return typeRef->type->kind() == CTypeKind::structType;
}

bool typeCheckVarStruct(CTypeRef *typeRef) {
  return typeRef->type->kind() == CTypeKind::varStructType;
}

bool typeCheckSubStruct(CTypeRef *typeRef) {
  return typeRef->type->kind() == CTypeKind::subStructType;
}

bool typeCheckEnum(CTypeRef *typeRef) {
  return typeRef->type->kind() == CTypeKind::enumType;
}

bool typeCheckContainer(CTypeRef *typeRef) {
  return typeRef->type->isContainer();
}

bool typeCheckPointer(CTypeRef *typeRef) {
  return typeRef->type->isPointer();
}

bool typeCheckOperand(CTypeRef *typeRef, TypeCheckKind kind) {
  switch (kind) {
  case TypeCheckKind::tInt:   return typeCheckInt(typeRef);
  case TypeCheckKind::tFloat: return typeCheckFloat(typeRef);
  case TypeCheckKind::tBool:  return typeCheckBool(typeRef);
  case TypeCheckKind::tEnum:  return typeCheckEnum(typeRef);
  default:                    return false;
  }
}
