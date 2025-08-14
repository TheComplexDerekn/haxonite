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
#include "TypeCheck.h"

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
	s += "V" + std::to_string(func->args.size());
	break;
      case CTypeKind::setType: {
	CTypeRef *keyType = arg0Type->params[0].get();
	switch (keyType->type->kind()) {
	case CTypeKind::intType:    s += "ZI" + std::to_string(func->args.size()); break;
	case CTypeKind::stringType: s += "ZS" + std::to_string(func->args.size()); break;
	default: break;
	}
	break;
      }
      case CTypeKind::mapType: {
	CTypeRef *keyType = arg0Type->params[0].get();
	switch (keyType->type->kind()) {
	case CTypeKind::intType:    s += "MI" + std::to_string(func->args.size()); break;
	case CTypeKind::stringType: s += "MS" + std::to_string(func->args.size()); break;
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

std::string mangleIntFormatFuncName() {
  return "format_IIII";
}

std::string mangleFloatFormatFuncName() {
  return "format_FIII";
}

std::string mangleBoolFormatFuncName() {
  return "format_BIII";
}

std::string mangleStringConcatFuncName() {
  return "concat_SS";
}

std::string mangleStringCompareFuncName() {
  return "compare_SS";
}

std::string mangleStringFormatFuncName() {
  return "format_SIII";
}

std::string mangleVectorAppendFuncName() {
  return "append_V2";
}

std::string mangleVectorIfirstFuncName() {
  return "ifirst_V1";
}

std::string mangleVectorImoreFuncName() {
  return "imore_V2";
}

std::string mangleVectorInextFuncName() {
  return "inext_V2";
}

std::string mangleVectorIgetFuncName() {
  return "iget_V2";
}

std::string mangleSetInsertFuncName(CTypeRef *elemType) {
  if (typeCheckString(elemType)) {
    return "insert_ZS2";
  } else if (typeCheckInt(elemType)) {
    return "insert_ZI2";
  } else {
    return "???";
  }
}

std::string mangleSetIfirstFuncName(CTypeRef *elemType) {
  if (typeCheckString(elemType)) {
    return "ifirst_ZS1";
  } else if (typeCheckInt(elemType)) {
    return "ifirst_ZI1";
  } else {
    return "???";
  }
}

std::string mangleSetImoreFuncName(CTypeRef *elemType) {
  if (typeCheckString(elemType)) {
    return "imore_ZS2";
  } else if (typeCheckInt(elemType)) {
    return "imore_ZI2";
  } else {
    return "???";
  }
}

std::string mangleSetInextFuncName(CTypeRef *elemType) {
  if (typeCheckString(elemType)) {
    return "inext_ZS2";
  } else if (typeCheckInt(elemType)) {
    return "inext_ZI2";
  } else {
    return "???";
  }
}

std::string mangleSetIgetFuncName(CTypeRef *elemType) {
  if (typeCheckString(elemType)) {
    return "iget_ZS2";
  } else if (typeCheckInt(elemType)) {
    return "iget_ZI2";
  } else {
    return "???";
  }
}

std::string mangleMapSetFuncName(CTypeRef *keyType) {
  if (typeCheckString(keyType)) {
    return "set_MS3";
  } else if (typeCheckInt(keyType)) {
    return "set_MI3";
  } else {
    return "???";
  }
}

std::string mangleMapIfirstFuncName(CTypeRef *keyType) {
  if (typeCheckString(keyType)) {
    return "ifirst_MS1";
  } else if (typeCheckInt(keyType)) {
    return "ifirst_MI1";
  } else {
    return "???";
  }
}

std::string mangleMapImoreFuncName(CTypeRef *keyType) {
  if (typeCheckString(keyType)) {
    return "imore_MS2";
  } else if (typeCheckInt(keyType)) {
    return "imore_MI2";
  } else {
    return "???";
  }
}

std::string mangleMapInextFuncName(CTypeRef *keyType) {
  if (typeCheckString(keyType)) {
    return "inext_MS2";
  } else if (typeCheckInt(keyType)) {
    return "inext_MI2";
  } else {
    return "???";
  }
}

std::string mangleMapIgetFuncName(CTypeRef *keyType) {
  if (typeCheckString(keyType)) {
    return "iget_MS2";
  } else if (typeCheckInt(keyType)) {
    return "iget_MI2";
  } else {
    return "???";
  }
}
