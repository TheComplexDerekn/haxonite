//========================================================================
//
// CTree.h
//
// Syntax tree types used by the compiler, after parsing into the AST.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef CTree_h
#define CTree_h

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "DateTime.h"
#include "Location.h"

class CType;
class CSubStructType;

//------------------------------------------------------------------------

class CModule {
public:

  CModule(const std::string &aName, bool aIsHeader, bool aIsNative, bool aBuiltin,
	  const std::string &aDir, const std::string &aSrcPath, const std::string &aObjPath,
	  DateTime aSrcTimestamp, DateTime aObjTimestamp)
    : name(aName), isHeader(aIsHeader), isNative(aIsNative), builtin(aBuiltin)
    , dir(aDir), srcPath(aSrcPath), objPath(aObjPath)
    , srcTimestamp(aSrcTimestamp), objTimestamp(aObjTimestamp)
  {}

  void addImport(CModule *cmod) { imports.push_back(cmod); }

  // for source modules:
  //   isHeader = false
  //   isNative = false
  //   srcPath = {dir}/src/{name}.hax
  //   objPath = {dir}/obj/{name}.haxo

  // for binary modules:
  //   isHeader = true
  //   isNative = false
  //   srcPath = {dir}/src/{name}.haxh
  //   objPath = {dir}/obj/{name}.haxo

  // for native modules:
  //   isHeader = true
  //   isNative = true
  //   srcPath = {dir}/src/{name}.haxh
  //   objPath = (unused)

  std::string name;
  bool isHeader;
  bool isNative;
  bool builtin;
  std::string dir;
  std::string srcPath;
  std::string objPath;
  DateTime srcTimestamp;
  DateTime objTimestamp;
  std::vector<CModule*> imports;
};

//------------------------------------------------------------------------

class CTypeRef {
public:

  CTypeRef(Location aLoc, const std::string &aName): loc(aLoc), name(aName), type(nullptr) {}
  CTypeRef(Location aLoc, CType *aType): loc(aLoc), type(aType) {}
  virtual ~CTypeRef() {}
  virtual CTypeRef *copy() = 0;
  virtual bool isParam() = 0;
  virtual std::string toString() = 0;

  Location loc;
  std::string name;		// valid until type refs are connected
  CType *type;			// valid after type refs are connected
};

//------------------------------------------------------------------------

class CSimpleTypeRef: public CTypeRef {
public:

  CSimpleTypeRef(Location aLoc, const std::string &aName): CTypeRef(aLoc, aName) {}
  CSimpleTypeRef(Location aLoc, CType *aType): CTypeRef(aLoc, aType) {}

  CTypeRef *copy()
    { return type ? new CSimpleTypeRef(loc, type)
	          : new CSimpleTypeRef(loc, name); }

  virtual bool isParam() { return false; }

  virtual std::string toString();
};

//------------------------------------------------------------------------

class CParamTypeRef: public CTypeRef {
public:

  CParamTypeRef(Location aLoc, const std::string &aName, bool aHasReturnType,
		std::vector<std::unique_ptr<CTypeRef>> aParams)
    : CTypeRef(aLoc, aName), hasReturnType(aHasReturnType), params(std::move(aParams)) {}

  CParamTypeRef(Location aLoc, CType *aType, bool aHasReturnType,
		std::vector<std::unique_ptr<CTypeRef>> aParams)
    : CTypeRef(aLoc, aType), hasReturnType(aHasReturnType), params(std::move(aParams)) {}

  CTypeRef *copy();

  virtual bool isParam() { return true; }

  virtual std::string toString();

  bool hasReturnType;
  std::vector<std::unique_ptr<CTypeRef>> params;
};

//------------------------------------------------------------------------

enum class CTypeKind {
  //--- CAtomicType
  intType,
  floatType,
  boolType,
  otherAtomicType,

  //--- CStringType
  stringType,
  stringBufType,

  //--- CPointerType
  otherPointerType,

  //--- CContainerType
  vectorType,
  setType,
  mapType,

  //--- CFuncType
  funcType,

  //--- CResultType
  resultType,

  //--- CStructType
  structType,

  //--- CVarStructType
  varStructType,

  //--- CSubStructType
  subStructType,

  //--- CEnumType
  enumType
};

//------------------------------------------------------------------------

enum class CParamKind {
  none,
  one,
  two,
  zeroOrMore,
  zeroOrOne
};

//------------------------------------------------------------------------

class CType {
public:

  CType(Location aLoc, bool aPub, const std::string &aName, CModule *aModule)
    : loc(aLoc), pub(aPub), name(aName), module(aModule) {}
  virtual ~CType() {}
  virtual CTypeKind kind() = 0;
  virtual bool isPointer() = 0;
  virtual bool isContainer() { return false; }
  virtual CParamKind paramKind() = 0;
  int minParams();
  int maxParams();

  Location loc;
  bool pub;
  std::string name;
  CModule *module;		// module where this type was defined
};

//------------------------------------------------------------------------

class CAtomicType: public CType {
public:

  CAtomicType(Location aLoc, bool aPub, const std::string &aName, CModule *aModule, CTypeKind aKind)
    : CType(aLoc, aPub, aName, aModule)
    , mKind(aKind) {}
  virtual CTypeKind kind() { return mKind; }
  virtual bool isPointer() { return false; }
  virtual CParamKind paramKind() { return CParamKind::none; }

private:

  CTypeKind mKind;
};

//------------------------------------------------------------------------

class CStringType: public CType {
public:

  CStringType(Location aLoc, bool aPub, const std::string &aName, CModule *aModule, CTypeKind aKind)
    : CType(aLoc, aPub, aName, aModule)
    , mKind(aKind) {}
  virtual CTypeKind kind() { return mKind; }
  virtual bool isPointer() { return true; }
  virtual CParamKind paramKind() { return CParamKind::none; }

private:

  CTypeKind mKind;
};

//------------------------------------------------------------------------

class CPointerType: public CType {
public:

  CPointerType(Location aLoc, bool aPub, const std::string &aName, CModule *aModule,
	       CTypeKind aKind)
    : CType(aLoc, aPub, aName, aModule)
    , mKind(aKind) {}
  virtual CTypeKind kind() { return mKind; }
  virtual bool isPointer() { return true; }
  virtual CParamKind paramKind() { return CParamKind::none; }

private:

  CTypeKind mKind;
};

//------------------------------------------------------------------------

class CContainerType: public CType {
public:

  CContainerType(Location aLoc, bool aPub, const std::string &aName, CModule *aModule,
		 CTypeKind aKind, CParamKind aParamKind)
    : CType(aLoc, aPub, aName, aModule)
    , mKind(aKind), mParamKind(aParamKind) {}
  virtual CTypeKind kind() { return mKind; }
  virtual bool isPointer() { return true; }
  virtual bool isContainer() { return true; }
  virtual CParamKind paramKind() { return mParamKind; }
  void addConcreteType(std::unique_ptr<CTypeRef> type);
  bool concreteTypeExists(CTypeRef *type);

private:

  CTypeKind mKind;
  CParamKind mParamKind;
  std::vector<std::unique_ptr<CTypeRef>> concreteTypes;
};

//------------------------------------------------------------------------

class CFuncType: public CType {
public:

  CFuncType(Location aLoc, bool aPub, const std::string &aName, CModule *aModule)
    : CType(aLoc, aPub, aName, aModule) {}
  virtual CTypeKind kind() { return CTypeKind::funcType; }
  virtual bool isPointer() { return true; }
  virtual CParamKind paramKind() { return CParamKind::zeroOrMore; }
};

//------------------------------------------------------------------------

class CResultType: public CType {
public:

  CResultType(Location aLoc, bool aPub, const std::string &aName, CModule *aModule)
    : CType(aLoc, aPub, aName, aModule) {}
  virtual CTypeKind kind() { return CTypeKind::resultType; }
  virtual bool isPointer() { return false; }
  virtual CParamKind paramKind() { return CParamKind::zeroOrOne; }
};

//------------------------------------------------------------------------

class CField {
public:

  CField(const std::string &aName, std::unique_ptr<CTypeRef> aType, int aFieldIdx)
    : name(aName), type(std::move(aType)), fieldIdx(aFieldIdx) {}

  std::string name;
  std::unique_ptr<CTypeRef> type;
  int fieldIdx;
};

//------------------------------------------------------------------------

class CStructType: public CType {
public:

  CStructType(Location aLoc, bool aPub, const std::string &aName, CModule *aModule,
	      std::unordered_map<std::string, std::unique_ptr<CField>> aFields)
    : CType(aLoc, aPub, aName, aModule)
    , fields(std::move(aFields)) {}
  virtual CTypeKind kind() { return CTypeKind::structType; }
  virtual bool isPointer() { return true; }
  virtual CParamKind paramKind() { return CParamKind::none; }

  std::unordered_map<std::string, std::unique_ptr<CField>> fields;
};

//------------------------------------------------------------------------

class CVarStructType: public CType {
public:

  CVarStructType(Location aLoc, bool aPub, const std::string &aName, CModule *aModule,
		 std::unordered_map<std::string, std::unique_ptr<CField>> aFields)
    : CType(aLoc, aPub, aName, aModule)
    , fields(std::move(aFields)) {}
  virtual CTypeKind kind() { return CTypeKind::varStructType; }
  virtual bool isPointer() { return true; }
  virtual CParamKind paramKind() { return CParamKind::none; }

  std::unordered_map<std::string, std::unique_ptr<CField>> fields;
  std::vector<CSubStructType*> subStructs;
};

//------------------------------------------------------------------------

class CSubStructType: public CType {
public:

  CSubStructType(Location aLoc, bool aPub, const std::string &aName, CModule *aModule,
		 CVarStructType *aParent, int aId,
		 std::unordered_map<std::string, std::unique_ptr<CField>> aFields)
    : CType(aLoc, aPub, aName, aModule)
    , parent(aParent), fields(std::move(aFields)), id(aId) {}
  virtual CTypeKind kind() { return CTypeKind::subStructType; }
  virtual bool isPointer() { return true; }
  virtual CParamKind paramKind() { return CParamKind::none; }

  CVarStructType *parent;
  int id;
  std::unordered_map<std::string, std::unique_ptr<CField>> fields;
};

//------------------------------------------------------------------------

class CEnumType: public CType {
public:

  CEnumType(Location aLoc, bool aPub, const std::string &aName, CModule *aModule,
	    std::unordered_map<std::string, int> aMembers)
    : CType(aLoc, aPub, aName, aModule)
    , members(std::move(aMembers)) {}
  virtual CTypeKind kind() { return CTypeKind::enumType; }
  virtual bool isPointer() { return false; }
  virtual CParamKind paramKind() { return CParamKind::none; }

  std::unordered_map<std::string, int> members;
};

//------------------------------------------------------------------------

class CConstValue {
public:

  virtual ~CConstValue() {}
  virtual std::unique_ptr<CConstValue> copy() = 0;
  virtual bool isInt() { return false; }
  virtual bool isFloat() { return false; }
  virtual bool isBool() { return false; }
  virtual bool isString() { return false; }
};

//------------------------------------------------------------------------

class CConstIntValue: public CConstValue {
public:

  CConstIntValue(int64_t aVal): val(aVal) {}
  virtual std::unique_ptr<CConstValue> copy() { return std::make_unique<CConstIntValue>(val); }
  virtual bool isInt() { return true; }

  int64_t val;
};

//------------------------------------------------------------------------

class CConstFloatValue: public CConstValue {
public:

  CConstFloatValue(float aVal): val(aVal) {}
  virtual std::unique_ptr<CConstValue> copy() { return std::make_unique<CConstFloatValue>(val); }
  virtual bool isFloat() { return true; }

  float val;
};

//------------------------------------------------------------------------

class CConstBoolValue: public CConstValue {
public:

  CConstBoolValue(bool aVal): val(aVal) {}
  virtual std::unique_ptr<CConstValue> copy() { return std::make_unique<CConstBoolValue>(val); }
  virtual bool isBool() { return true; }

  bool val;
};

//------------------------------------------------------------------------

class CConstStringValue: public CConstValue {
public:

  CConstStringValue(const std::string &aVal): val(aVal) {}
  virtual std::unique_ptr<CConstValue> copy() { return std::make_unique<CConstStringValue>(val); }
  virtual bool isString() { return true; }

  std::string val;
};

//------------------------------------------------------------------------

enum class CSymbolKind {
  constant,
  arg,
  var
};

//------------------------------------------------------------------------

class CSymbol {
public:

  CSymbol(Location aLoc, const std::string &aName, std::unique_ptr<CTypeRef> aType)
    : loc(aLoc), name(aName), type(std::move(aType)) {}
  virtual ~CSymbol() {}
  virtual CSymbolKind kind() = 0;
  virtual bool isWritable() = 0;

  Location loc;
  std::string name;
  std::unique_ptr<CTypeRef> type;
};

//------------------------------------------------------------------------

class CConst: public CSymbol {
public:

  CConst(Location aLoc, bool aPub, const std::string &aName, CModule *aModule,
	 std::unique_ptr<CTypeRef> aType, std::unique_ptr<CConstValue> aValue)
    : CSymbol(aLoc, aName, std::move(aType))
    , module(aModule)
    , pub(aPub)
    , value(std::move(aValue)) {}
  virtual CSymbolKind kind() { return CSymbolKind::constant; }
  virtual bool isWritable() { return false; }

  CModule *module;
  bool pub;
  std::unique_ptr<CConstValue> value;
};

//------------------------------------------------------------------------

class CArg: public CSymbol {
public:

  CArg(Location aLoc, const std::string &aName, std::unique_ptr<CTypeRef> aType, int aArgIdx)
    : CSymbol(aLoc, aName, std::move(aType))
    , argIdx(aArgIdx) {}
  CArg(CArg *other)
    : CSymbol(other->loc, other->name, std::unique_ptr<CTypeRef>(other->type->copy()))
    , argIdx(other->argIdx) {}
  virtual CSymbolKind kind() { return CSymbolKind::arg; }
  virtual bool isWritable() { return false; }

  int argIdx;
};

//------------------------------------------------------------------------

class CVar: public CSymbol {
public:

  CVar(Location aLoc, const std::string &aName, std::unique_ptr<CTypeRef> aType, int aFrameIdx,
       bool aWritable)
    : CSymbol(aLoc, aName, std::move(aType)), frameIdx(aFrameIdx), writable(aWritable) {}
  virtual CSymbolKind kind() { return CSymbolKind::var; }
  virtual bool isWritable() { return writable; }

  int frameIdx;
  bool writable;
};

//------------------------------------------------------------------------

class CFuncDecl {
public:

  CFuncDecl(Location aLoc, bool aPub, bool aNative, bool aBuiltinContainerType,
	    const std::string &aName, CModule *aModule,
	    std::vector<std::unique_ptr<CArg>> aArgs, std::unique_ptr<CTypeRef> aReturnType)
    : loc(aLoc), native(aNative), pub(aPub)
    , builtinContainerType(aBuiltinContainerType)
    , name(aName), module(aModule)
    , args(std::move(aArgs)), returnType(std::move(aReturnType)) {}

  Location loc;
  bool pub;
  bool native;
  bool builtinContainerType;
  std::string name;
  CModule *module;
  std::vector<std::unique_ptr<CArg>> args;
  std::unique_ptr<CTypeRef> returnType;
};

//------------------------------------------------------------------------

// Result of generating code for an expression.
//
// On error: ok=false; type is invalid.
// On success, with no value: ok=true, type=null.
// On success, with value: ok=true, type = value type.
struct ExprResult {
  ExprResult(): ok(false) {}
  explicit ExprResult(std::unique_ptr<CTypeRef> aType): ok(true), type(std::move(aType)) {}

  bool ok;
  std::unique_ptr<CTypeRef> type;
};

#endif // CTree_h
