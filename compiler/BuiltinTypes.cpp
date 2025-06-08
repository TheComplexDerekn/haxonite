//========================================================================
//
// BuiltinTypes.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "BuiltinTypes.h"

//------------------------------------------------------------------------

void addBuiltinTypes(Context &ctx) {
  CModule *haxModule = ctx.findModule("haxonite");

  ctx.intType = new CAtomicType(Location(), "Int", haxModule, CTypeKind::intType);
  ctx.addType(std::unique_ptr<CType>(ctx.intType));

  ctx.floatType = new CAtomicType(Location(), "Float", haxModule, CTypeKind::floatType);
  ctx.addType(std::unique_ptr<CType>(ctx.floatType));

  ctx.boolType = new CAtomicType(Location(), "Bool", haxModule, CTypeKind::boolType);
  ctx.addType(std::unique_ptr<CType>(ctx.boolType));

  ctx.stringType = new CStringType(Location(), "String", haxModule, CTypeKind::stringType);
  ctx.addType(std::unique_ptr<CType>(ctx.stringType));

  ctx.stringBufType = new CStringType(Location(), "StringBuf", haxModule, CTypeKind::stringBufType);
  ctx.addType(std::unique_ptr<CType>(ctx.stringBufType));

  ctx.vectorType = new CContainerType(Location(), "Vector", haxModule,
				      CTypeKind::vectorType, CParamKind::one);
  ctx.addType(std::unique_ptr<CType>(ctx.vectorType));

  ctx.setType = new CContainerType(Location(), "Set", haxModule,
				   CTypeKind::setType, CParamKind::one);
  ctx.addType(std::unique_ptr<CType>(ctx.setType));

  ctx.mapType = new CContainerType(Location(), "Map", haxModule,
				   CTypeKind::mapType, CParamKind::two);
  ctx.addType(std::unique_ptr<CType>(ctx.mapType));

  ctx.funcType = new CFuncType(Location(), "Func", haxModule);
  ctx.addType(std::unique_ptr<CType>(ctx.funcType));

  ctx.resultType = new CResultType(Location(), "Result", haxModule);
  ctx.addType(std::unique_ptr<CType>(ctx.resultType));
}
