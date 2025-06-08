//========================================================================
//
// CTree.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "CTree.h"
#include "TypeCheck.h"

//------------------------------------------------------------------------

int CType::minParams() {
  switch (paramKind()) {
  case CParamKind::none:       return 0;
  case CParamKind::one:        return 1;
  case CParamKind::two:        return 2;
  case CParamKind::zeroOrMore: return 0;
  case CParamKind::zeroOrOne:  return 0;
  default:                     return 0; // this should never happen
  }
}

int CType::maxParams() {
  switch (paramKind()) {
  case CParamKind::none:       return 0;
  case CParamKind::one:        return 1;
  case CParamKind::two:        return 2;
  case CParamKind::zeroOrMore: return -1;
  case CParamKind::zeroOrOne:  return 1;
  default:                     return 0; // this should never happen
  }
}

//------------------------------------------------------------------------

void CContainerType::addConcreteType(std::unique_ptr<CTypeRef> type) {
  concreteTypes.push_back(std::move(type));
}

bool CContainerType::concreteTypeExists(CTypeRef *type) {
  for (std::unique_ptr<CTypeRef> &concreteType : concreteTypes) {
    if (typeMatch(type, concreteType.get())) {
      return true;
    }
  }
  return false;
}

//------------------------------------------------------------------------

std::string CSimpleTypeRef::toString() {
  return type->name;
}

//------------------------------------------------------------------------

CTypeRef *CParamTypeRef::copy() {
  std::vector<std::unique_ptr<CTypeRef>> aParams;
  for (std::unique_ptr<CTypeRef> &param : params) {
    aParams.push_back(std::unique_ptr<CTypeRef>(param->copy()));
  }
  return type ? new CParamTypeRef(loc, type, hasReturnType, std::move(aParams))
              : new CParamTypeRef(loc, name, hasReturnType, std::move(aParams));
}

std::string CParamTypeRef::toString() {
  std::string s = type->name + "[";
  size_t n = params.size();
  for (size_t i = 0; i < n; ++i) {
    if (hasReturnType && i == n - 1) {
      s += "->";
    } else if (i > 0) {
      s += ",";
    }
    s += params[i]->toString();
  }
  s += "]";
  return s;
}
