//========================================================================
//
// Mangle.h
//
// Function name mangler.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef Mangle_h
#define Mangle_h

#include <string>
#include "CTree.h"

//------------------------------------------------------------------------

extern std::string mangleFunctionName(CFuncDecl *func);

extern std::string mangleIntFormatFuncName();
extern std::string mangleFloatFormatFuncName();
extern std::string mangleBoolFormatFuncName();

extern std::string mangleStringConcatFuncName();
extern std::string mangleStringCompareFuncName();
extern std::string mangleStringFormatFuncName();

extern std::string mangleVectorAppendFuncName();
extern std::string mangleVectorIfirstFuncName();
extern std::string mangleVectorImoreFuncName();
extern std::string mangleVectorInextFuncName();
extern std::string mangleVectorIgetFuncName();

extern std::string mangleSetInsertFuncName(CTypeRef *elemType);
extern std::string mangleSetIfirstFuncName(CTypeRef *elemType);
extern std::string mangleSetImoreFuncName(CTypeRef *elemType);
extern std::string mangleSetInextFuncName(CTypeRef *elemType);
extern std::string mangleSetIgetFuncName(CTypeRef *elemType);

extern std::string mangleMapSetFuncName(CTypeRef *keyType);
extern std::string mangleMapIfirstFuncName(CTypeRef *keyType);
extern std::string mangleMapImoreFuncName(CTypeRef *keyType);
extern std::string mangleMapInextFuncName(CTypeRef *keyType);
extern std::string mangleMapIgetFuncName(CTypeRef *keyType);

#endif // Mangle_h
