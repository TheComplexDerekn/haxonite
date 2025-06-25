//========================================================================
//
// Mangle.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "Mangle.h"
#include "Error.h"

//------------------------------------------------------------------------

static std::string mangleTypeRef(CTypeRef *typeRef);

//------------------------------------------------------------------------

std::string mangleFunctionName(CFuncDecl *func) {
  std::string s;
  if (func->builtinContainerType) {
    s = func->name + "_";
    if (func->args[0]->type->isParam()) {
      CParamTypeRef *arg0Type = (CParamTypeRef *)func->args[0]->type.get();
      switch (arg0Type->type->kind()) {
      case CTypeKind::vectorType:
	s += "V";
	break;
      case CTypeKind::setType: {
	CTypeRef *keyType = arg0Type->params[0].get();
	switch (keyType->type->kind()) {
	case CTypeKind::intType:    s += "ZI"; break;
	case CTypeKind::stringType: s += "ZS"; break;
	default: break;
	}
	break;
      }
      case CTypeKind::mapType: {
	CTypeRef *keyType = arg0Type->params[0].get();
	switch (keyType->type->kind()) {
	case CTypeKind::intType:    s += "MI"; break;
	case CTypeKind::stringType: s += "MS"; break;
	default: break;
	}
	break;
      }
      default:
	break;
      }
    }
  } else {
    s = func->name;
    if (!func->args.empty()) {
      s.push_back('_');
      for (std::unique_ptr<CArg> &arg : func->args) {
	s.append(mangleTypeRef(arg->type.get()));
      }
    }
  }
  return s;
}

static std::string mangleTypeRef(CTypeRef *typeRef) {
  if (typeRef->isParam()) {
    CParamTypeRef *paramTypeRef = (CParamTypeRef *)typeRef;
    switch (paramTypeRef->type->kind()) {
    case CTypeKind::vectorType:
      return "V" + mangleTypeRef(paramTypeRef->params[0].get());
    case CTypeKind::setType:
      return "Z" + mangleTypeRef(paramTypeRef->params[0].get());
    case CTypeKind::mapType:
      return "M" + mangleTypeRef(paramTypeRef->params[0].get()) +
	           mangleTypeRef(paramTypeRef->params[1].get());
    case CTypeKind::funcType: {
      std::string s = "G" + std::to_string(paramTypeRef->params.size());
      s += paramTypeRef->hasReturnType ? "R" : "N";
      for (std::unique_ptr<CTypeRef> &argType : paramTypeRef->params) {
	s += mangleTypeRef(argType.get());
      }
      return s;
    }
    case CTypeKind::resultType:
      return "R" + mangleTypeRef(paramTypeRef->params[0].get());
    default:
      error(paramTypeRef->loc, "Internal error: mangleTypeRef");
      return "ZZZ";
    }
  } else {
    CSimpleTypeRef *simpleTypeRef = (CSimpleTypeRef *)typeRef;
    switch (simpleTypeRef->type->kind()) {
    case CTypeKind::intType:
      return "I";
    case CTypeKind::floatType:
      return "F";
    case CTypeKind::boolType:
      return "B";
    case CTypeKind::stringType:
      return "S";
    case CTypeKind::stringBufType:
      return "T";
    case CTypeKind::otherAtomicType:
    case CTypeKind::otherPointerType:
    case CTypeKind::structType:
    case CTypeKind::varStructType:
    case CTypeKind::subStructType:
    case CTypeKind::enumType:
      return std::to_string(simpleTypeRef->type->name.size()) + simpleTypeRef->type->name;
    default:
      error(simpleTypeRef->loc, "Internal error: mangleTypeRef");
      return "ZZZ";
    }
  }
}
