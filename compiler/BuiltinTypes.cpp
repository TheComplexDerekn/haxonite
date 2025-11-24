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

  ctx.intType = new CAtomicType(Location(), true, "Int", haxModule, CTypeKind::intType);
  ctx.addType(std::unique_ptr<CType>(ctx.intType));

  ctx.floatType = new CAtomicType(Location(), true, "Float", haxModule, CTypeKind::floatType);
  ctx.addType(std::unique_ptr<CType>(ctx.floatType));

  ctx.boolType = new CAtomicType(Location(), true, "Bool", haxModule, CTypeKind::boolType);
  ctx.addType(std::unique_ptr<CType>(ctx.boolType));

  ctx.stringType = new CStringType(Location(), true, "String", haxModule, CTypeKind::stringType);
  ctx.addType(std::unique_ptr<CType>(ctx.stringType));

  ctx.stringBufType = new CStringType(Location(), true, "StringBuf", haxModule,
				      CTypeKind::stringBufType);
  ctx.addType(std::unique_ptr<CType>(ctx.stringBufType));

  ctx.vectorType = new CContainerType(Location(), true, "Vector", haxModule,
				      CTypeKind::vectorType, CParamKind::one);
  ctx.addType(std::unique_ptr<CType>(ctx.vectorType));

  ctx.setType = new CContainerType(Location(), true, "Set", haxModule,
				   CTypeKind::setType, CParamKind::one);
  ctx.addType(std::unique_ptr<CType>(ctx.setType));

  ctx.mapType = new CContainerType(Location(), true, "Map", haxModule,
				   CTypeKind::mapType, CParamKind::two);
  ctx.addType(std::unique_ptr<CType>(ctx.mapType));

  ctx.funcType = new CFuncType(Location(), true, "Func", haxModule);
  ctx.addType(std::unique_ptr<CType>(ctx.funcType));

  ctx.resultType = new CResultType(Location(), true, "Result", haxModule);
  ctx.addType(std::unique_ptr<CType>(ctx.resultType));
}
