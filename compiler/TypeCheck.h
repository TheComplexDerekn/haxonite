//========================================================================
//
// TypeCheck.h
//
// Type-checking.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef TypeCheck_h
#define TypeCheck_h

#include <vector>
#include "CodeGenExpr.h"
#include "CTree.h"

//------------------------------------------------------------------------

// Used to describe operands and results of binary/unary operators.
enum class TypeCheckKind {
  tInt,		// Int
  tFloat,	// Float
  tBool,	// Bool
  tEnum		// enum type
};

//------------------------------------------------------------------------

// Return true if [funcDecl1] has the same arg types as [funcDecl2].
extern bool functionCollision(CFuncDecl *funcDecl1, CFuncDecl *funcDecl2);

// Return true if [argTypes] match the arg types in [funcDecl].
extern bool functionMatch(std::vector<std::unique_ptr<CTypeRef>> &argTypes, CFuncDecl *funcDecl);

// Return true if [argTypes] match the arg types in [funcDecl].
extern bool functionMatch(std::vector<ExprResult> &argResults, CFuncDecl *funcDecl);

// Return true if [typeRef1] matches [typeRef2].
extern bool typeMatch(CTypeRef *typeRef1, CTypeRef *typeRef2);

// Return true if [typeRef] is Int.
extern bool typeCheckInt(CTypeRef *typeRef);

// Return true if [typeRef] is Bool.
extern bool typeCheckBool(CTypeRef *typeRef);

// Return true if [typeRef] is Float.
extern bool typeCheckFloat(CTypeRef *typeRef);

// Return true if [typeRef] is String.
extern bool typeCheckString(CTypeRef *typeRef);

// Return true if [typeRef] is StringBuf.
extern bool typeCheckStringBuf(CTypeRef *typeRef);

// Return true if [typeRef] is Vector.
extern bool typeCheckVector(CTypeRef *typeRef);

// Return true if [typeRef] is Set.
extern bool typeCheckSet(CTypeRef *typeRef);

// Return true if [typeRef] is Map.
extern bool typeCheckMap(CTypeRef *typeRef);

// Return true if [typeRef] is Func.
extern bool typeCheckFuncPointer(CTypeRef *typeRef);

// Return true if [typeRef] is Result[T].
extern bool typeCheckResult(CTypeRef *typeRef);

// Return true if [typeRef] is a struct type.
extern bool typeCheckStruct(CTypeRef *typeRef);

// Return true if [typeRef] is a varstruct type.
extern bool typeCheckVarStruct(CTypeRef *typeRef);

// Return true if [typeRef] is a substruct type.
extern bool typeCheckSubStruct(CTypeRef *typeRef);

// Return true if [typeRef] is an enum type.
extern bool typeCheckEnum(CTypeRef *typeRef);

// Return true if [typeRef] is a container type.
extern bool typeCheckContainer(CTypeRef *typeRef);

// Return true if [typeRef] is a pointer type.
extern bool typeCheckPointer(CTypeRef *typeRef);

// Return true if [typeRef] matches [kind].
extern bool typeCheckOperand(CTypeRef *typeRef, TypeCheckKind kind);

#endif // TypeCheck_h
