//========================================================================
//
// ModuleScanner.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "ModuleScanner.h"
#include <limits.h>
#include "ConstExpr.h"
#include "CTree.h"
#include "DateTime.h"
#include "Error.h"
#include "FileNames.h"
#include "Parser.h"
#include "SysIO.h"

//------------------------------------------------------------------------

static CModule *scanModule(const std::string &moduleName, bool builtin, Context &ctx);
static std::unique_ptr<Module> scanContainerTypeHeader(const std::string &headerName,
						       Context &ctx);
static bool findModule(Context &ctx, const std::string &moduleName,
		       bool &isHeader, bool &isNative, std::string &moduleDir,
		       std::string &srcPath, std::string &objPath,
		       DateTime &srcTimestamp, DateTime &objTimestamp);
static bool scanStructDefn(StructDefn *structDefn, CModule *cmod, Context &ctx);
static bool scanVarStructDefn(VarStructDefn *varStructDefn, CModule *cmod, Context &ctx);
static bool scanEnumDefn(EnumDefn *enumDefn, CModule *cmod, Context &ctx);
static bool scanNativeTypeDefn(NativeTypeDefn *nativeTypeDefn, CModule *cmod, Context &ctx);
static bool scanConstDefn(ConstDefn *constDefn, CModule *cmod, Context &ctx);
static bool scanFuncDefn(FuncDefn *funcDefn, CModule *cmod, Context &ctx);
static std::unique_ptr<CTypeRef> scanTypeRef(TypeRef *typeRef);

//------------------------------------------------------------------------

bool scanBuiltinModule(const std::string &moduleName, Context &ctx) {
  return scanModule(moduleName, true, ctx) != nullptr;
}

bool scanContainerTypeHeaders(const std::string &vectorHeaderName,
			      const std::string &setHeaderName,
			      const std::string &mapHeaderName,
			      Context &ctx) {
  ctx.vectorHeader = scanContainerTypeHeader(vectorHeaderName, ctx);
  ctx.setHeader = scanContainerTypeHeader(setHeaderName, ctx);
  ctx.mapHeader = scanContainerTypeHeader(mapHeaderName, ctx);
  return ctx.vectorHeader != nullptr &&
         ctx.setHeader != nullptr &&
         ctx.mapHeader != nullptr;
}

bool scanModules(const std::string &topModuleName, Context &ctx) {
  ctx.topModule = scanModule(topModuleName, false, ctx);
  return ctx.topModule != nullptr;
}

static CModule *scanModule(const std::string &moduleName, bool builtin, Context &ctx) {
  CModule *cmod = ctx.findModule(moduleName);
  if (cmod) {
    return cmod;
  }

  bool isHeader, isNative;
  std::string moduleDir, srcPath, objPath;
  DateTime srcTimestamp, objTimestamp;
  if (!findModule(ctx, moduleName, isHeader, isNative, moduleDir, srcPath, objPath,
		  srcTimestamp, objTimestamp)) {
    error(Location(), "Couldn't find module '%s'", moduleName.c_str());
    return nullptr;
  }

  std::string moduleContents;
  if (!readFile(srcPath, moduleContents)) {
    error(Location(), "Couldn't read source file '%s'", srcPath.c_str());
    return nullptr;
  }
  if (ctx.verbose) {
    printf("scanning module %s\n", srcPath.c_str());
  }
  Parser parser(moduleContents, makeShortFileName(moduleDir, moduleName, isHeader));
  std::unique_ptr<Module> module;
  if (isHeader) {
    module = parser.parseHeader();
  } else {
    module = parser.parseModule();
  }
  if (!module) {
    return nullptr;
  }

  if (module->name != moduleName) {
    error(module->loc, "Module name doesn't match file name");
    return nullptr;
  }

  cmod = new CModule(moduleName, isHeader, isNative, builtin,
		     moduleDir, srcPath, objPath, srcTimestamp, objTimestamp);
  ctx.addModule(std::unique_ptr<CModule>(cmod));

  bool ok = true;
  for (std::unique_ptr<Import> &import : module->imports) {
    CModule *cImport = scanModule(import->name, false, ctx);
    if (cImport) {
      cmod->addImport(cImport);
    } else {
      ok = false;
    }
  }

  ctx.moduleBeingCompiled = cmod;
  for (std::unique_ptr<ModuleElem> &elem : module->elems) {
    switch (elem->kind()) {
    case ModuleElem::Kind::structDefn:
      ok &= scanStructDefn((StructDefn *)elem.get(), cmod, ctx);
      break;
    case ModuleElem::Kind::varStructDefn:
      ok &= scanVarStructDefn((VarStructDefn *)elem.get(), cmod, ctx);
      break;
    case ModuleElem::Kind::enumDefn:
      ok &= scanEnumDefn((EnumDefn *)elem.get(), cmod, ctx);
      break;
    case ModuleElem::Kind::nativeTypeDefn:
      ok &= scanNativeTypeDefn((NativeTypeDefn *)elem.get(), cmod, ctx);
      break;
    case ModuleElem::Kind::constDefn:
      ok &= scanConstDefn((ConstDefn *)elem.get(), cmod, ctx);
      break;
    case ModuleElem::Kind::funcDefn:
      ok &= scanFuncDefn((FuncDefn *)elem.get(), cmod, ctx);
      break;
    }
  }
  ctx.moduleBeingCompiled = nullptr;

  return cmod;
}

static std::unique_ptr<Module> scanContainerTypeHeader(const std::string &headerName,
						       Context &ctx) {
  bool isHeader, isNative;
  std::string moduleDir, srcPath, objPath;
  DateTime srcTimestamp, objTimestamp;
  if (!findModule(ctx, headerName, isHeader, isNative, moduleDir, srcPath, objPath,
		  srcTimestamp, objTimestamp)) {
    error(Location(), "Couldn't find module '%s'", headerName.c_str());
    return nullptr;
  }
  if (!isHeader || !isNative) {
    error(Location(), "Invalid builtin container header for '%s'", headerName.c_str());
    return nullptr;
  }

  std::string moduleContents;
  if (!readFile(srcPath, moduleContents)) {
    error(Location(), "Couldn't read source file '%s'", srcPath.c_str());
    return nullptr;
  }
  if (ctx.verbose) {
    printf("scanning module %s\n", srcPath.c_str());
  }
  Parser parser(moduleContents, makeShortFileName(moduleDir, headerName, true));
  std::unique_ptr<Module> module;
  return parser.parseHeader();
}

// Search for [moduleName], using [ctx].searchPath.
// If found:
//   - set [isHeader] to true for a .haxh file, false for .hax
//   - set [isNative] to true if the .haxh file exists and the .haxo
//     file does not
//   - set [moduleDir] to the module dir
//   - set [srcPath] to the source file path
//     ([moduleDir]/src/[module].hax or [moduleDir]/hdr/[module].haxh)
//   - set [objPath] to the object file path
//     ([moduleDir]/obj/[module].haxo - this file may or may not exist)
//   - set [srcTimestamp] to [srcPath]'s timestamp
//   - set [objTimestamp] to [objPath]'s timestamp; invalid if the
//     object file doesn't exist
//   - return true
// If not found:
//   - return false
static bool findModule(Context &ctx, const std::string &moduleName,
		       bool &isHeader, bool &isNative, std::string &moduleDir,
		       std::string &srcPath, std::string &objPath,
		       DateTime &srcTimestamp, DateTime &objTimestamp) {
  for (const std::string dir : ctx.searchPath) {
    std::string path = makeSourceFileName(dir, moduleName);
    if (pathIsFile(path)) {
      isHeader = false;
      isNative = false;
      moduleDir = dir;
      srcPath = path;
      objPath = makeObjectFileName(dir, moduleName);
      srcTimestamp = pathModTime(srcPath);
      objTimestamp = pathModTime(objPath);
      return true;
    }
    path = makeHeaderFileName(dir, moduleName);
    if (pathIsFile(path)) {
      isHeader = true;
      moduleDir = dir;
      srcPath = path;
      objPath = makeObjectFileName(dir, moduleName);
      isNative = !pathIsFile(objPath);
      srcTimestamp = pathModTime(srcPath);
      objTimestamp = pathModTime(objPath);
      return true;
    }
  }
  return false;
}

static bool scanStructDefn(StructDefn *structDefn, CModule *cmod, Context &ctx) {
  if (ctx.nameExists(structDefn->name)) {
    error(structDefn->loc, "Type '%s' duplicates an existing name", structDefn->name.c_str());
    return false;
  }

  bool ok = true;

  std::unordered_map<std::string, std::unique_ptr<CField>> cFields;
  for (size_t i = 0; i < structDefn->fields.size(); ++i) {
    Field *field = structDefn->fields[i].get();
    if (cFields.count(field->name)) {
      error(field->loc, "Duplicate field name '%s' in struct '%s'",
	    field->name.c_str(), structDefn->name.c_str());
      ok = false;
    }
    std::unique_ptr<CTypeRef> cTypeRef = scanTypeRef(field->type.get());
    if (cTypeRef) {
      cFields[field->name] = std::make_unique<CField>(field->name, std::move(cTypeRef), (int)i);
    } else {
      ok = false;
    }
  }

  if (!ok) {
    return false;
  }

  ctx.addType(std::make_unique<CStructType>(structDefn->loc, structDefn->name, cmod,
					    std::move(cFields)));
  return true;
}

static bool scanVarStructDefn(VarStructDefn *varStructDefn, CModule *cmod, Context &ctx) {
  if (ctx.nameExists(varStructDefn->name)) {
    error(varStructDefn->loc, "Type '%s' duplicates an existing name", varStructDefn->name.c_str());
    return false;
  }

  bool ok = true;

  std::unordered_map<std::string, std::unique_ptr<CField>> cFields;
  int fieldIdx = 1;  // field 0 is the substruct ID
  for (size_t i = 0; i < varStructDefn->fields.size(); ++i) {
    Field *field = varStructDefn->fields[i].get();
    if (cFields.count(field->name)) {
      error(field->loc, "Duplicate field name '%s' in varstruct '%s'",
	    field->name.c_str(), varStructDefn->name.c_str());
      ok = false;
    }
    std::unique_ptr<CTypeRef> cTypeRef = scanTypeRef(field->type.get());
    if (cTypeRef) {
      cFields[field->name] = std::make_unique<CField>(field->name, std::move(cTypeRef), fieldIdx++);
    } else {
      ok = false;
    }
  }

  std::unique_ptr<CVarStructType> cVarStruct =
      std::make_unique<CVarStructType>(varStructDefn->loc, varStructDefn->name, cmod,
				       std::move(cFields));

  std::vector<CSubStructType*> cSubStructs;
  if (varStructDefn->subStructs.size() > INT_MAX) {
    error(varStructDefn->loc, "Too many substructs in varstruct");
    return false;
  }
  for (size_t subStructIdx = 0; subStructIdx < varStructDefn->subStructs.size(); ++subStructIdx) {
    SubStructDefn *subStructDefn = varStructDefn->subStructs[subStructIdx].get();
    if (ctx.nameExists(subStructDefn->name)) {
      error(varStructDefn->loc, "Type '%s' duplicates an existing name",
	    subStructDefn->name.c_str());
      ok = false;
    }
    std::unordered_map<std::string, std::unique_ptr<CField>> cSubFields;
    int subFieldIdx = fieldIdx;
    for (size_t i = 0; i < subStructDefn->fields.size(); ++i) {
      Field *field = subStructDefn->fields[i].get();
      if (cVarStruct->fields.count(field->name) || cSubFields.count(field->name)) {
	error(field->loc, "Duplicate field name '%s' in substruct '%s'",
	      field->name.c_str(), subStructDefn->name.c_str());
	ok = false;
      }
      std::unique_ptr<CTypeRef> cTypeRef = scanTypeRef(field->type.get());
      if (cTypeRef) {
	cSubFields[field->name] = std::make_unique<CField>(field->name, std::move(cTypeRef),
							   subFieldIdx++);
      } else {
	ok = false;
      }
    }
    std::unique_ptr<CSubStructType> cSubStruct =
        std::make_unique<CSubStructType>(subStructDefn->loc, subStructDefn->name, cmod,
					 cVarStruct.get(), (int)subStructIdx,
					 std::move(cSubFields));
    cVarStruct->subStructs.push_back(cSubStruct.get());
    ctx.addType(std::move(cSubStruct));
  }

  if (!ok) {
    return false;
  }

  ctx.addType(std::move(cVarStruct));
  return true;
}

static bool scanEnumDefn(EnumDefn *enumDefn, CModule *cmod, Context &ctx) {
  if (ctx.nameExists(enumDefn->name)) {
    error(enumDefn->loc, "Type '%s' duplicates an existing name", enumDefn->name.c_str());
    return false;
  }

  bool ok = true;

  std::unordered_map<std::string, int> cMembers;
  if (enumDefn->members.size() > INT_MAX) {
    error(enumDefn->loc, "Too many members in enum");
    return false;
  }
  for (size_t i = 0; i < enumDefn->members.size(); ++i) {
    std::string member = enumDefn->members[i];
    if (cMembers.count(member)) {
      error(enumDefn->loc, "Duplicate member name '%s' in enum '%s'",
	    member.c_str(), enumDefn->name.c_str());
      ok = false;
    } else {
      cMembers[member] = (int)i;
    }
  }

  if (!ok) {
    return false;
  }

  ctx.addType(std::make_unique<CEnumType>(enumDefn->loc, enumDefn->name, cmod,
					  std::move(cMembers)));
  return true;
}

static bool scanNativeTypeDefn(NativeTypeDefn *nativeTypeDefn, CModule *cmod, Context &ctx) {
  if (ctx.nameExists(nativeTypeDefn->name)) {
    error(nativeTypeDefn->loc, "Type '%s' duplicates an existing name",
	  nativeTypeDefn->name.c_str());
    return false;
  }

  bool atomic = false;
  bool pointer = false;
  int n = 0;
  for (const std::string &attr : nativeTypeDefn->attrs) {
    if (attr == "atomic") {
      atomic = true;
      ++n;
    } else if (attr == "pointer") {
      pointer = true;
      ++n;
    } else {
      error(nativeTypeDefn->loc, "Invalid attribute '%s' on native type", attr.c_str());
      return false;
    }
  }
  if (n != 1) {
    error(nativeTypeDefn->loc, "Native type definitions must specify exactly one of the 'atomic' or 'pointer' attributes");
    return false;
  }

  if (atomic) {
    ctx.addType(std::make_unique<CAtomicType>(Location(), nativeTypeDefn->name, cmod,
					      CTypeKind::otherAtomicType));
  } else {
    ctx.addType(std::make_unique<CPointerType>(Location(), nativeTypeDefn->name, cmod,
					       CTypeKind::otherPointerType));
  }

  return true;
}

static bool scanConstDefn(ConstDefn *constDefn, CModule *cmod, Context &ctx) {
  if (ctx.nameExists(constDefn->name)) {
    error(constDefn->loc, "Constant '%s' duplicates an existing name", constDefn->name.c_str());
    return false;
  }
  std::unique_ptr<CConstValue> value = evalConstExpr(constDefn->val.get(), ctx);
  std::unique_ptr<CTypeRef> type;
  if (value->isInt()) {
    type = std::make_unique<CSimpleTypeRef>(constDefn->loc, "Int");
  } else if (value->isFloat()) {
    type = std::make_unique<CSimpleTypeRef>(constDefn->loc, "Float");
  } else if (value->isBool()) {
    type = std::make_unique<CSimpleTypeRef>(constDefn->loc, "Bool");
  } else if (value->isString()) {
    type = std::make_unique<CSimpleTypeRef>(constDefn->loc, "String");
  } else {
    error(constDefn->loc, "Internal (scanConstDefn)");
    return false;
  }
  ctx.addConst(std::make_unique<CConst>(constDefn->loc, constDefn->name, cmod, std::move(type),
					std::move(value)));
  return true;
}

static bool scanFuncDefn(FuncDefn *funcDefn, CModule *cmod, Context &ctx) {
  bool ok = true;

  std::vector<std::unique_ptr<CArg>> cArgs;
  int argIdx = 0;
  for (std::unique_ptr<Arg> &arg : funcDefn->args) {
    std::unique_ptr<CTypeRef> cTypeRef = scanTypeRef(arg->type.get());
    if (cTypeRef) {
      cArgs.push_back(std::make_unique<CArg>(arg->loc, arg->name, std::move(cTypeRef), argIdx));
    } else {
      ok = false;
    }
    ++argIdx;
  }

  std::unique_ptr<CTypeRef> cReturnType;
  if (funcDefn->returnType) {
    cReturnType = scanTypeRef(funcDefn->returnType.get());
    if (!cReturnType) {
      ok = false;
    }
  }

  if (!ok) {
    return false;
  }
  ctx.addFunc(std::make_unique<CFuncDecl>(funcDefn->loc, funcDefn->native, false, funcDefn->name,
					  cmod, std::move(cArgs), std::move(cReturnType)));
  return true;
}

// Convert a TypeRef to a CTypeRef.
static std::unique_ptr<CTypeRef> scanTypeRef(TypeRef *typeRef) {
  switch (typeRef->kind()) {
  case TypeRef::Kind::simpleTypeRef: {
    SimpleTypeRef *simpleTypeRef = (SimpleTypeRef *)typeRef;
    return std::make_unique<CSimpleTypeRef>(simpleTypeRef->loc, simpleTypeRef->name);
  }
  case TypeRef::Kind::paramTypeRef: {
    ParamTypeRef *paramTypeRef = (ParamTypeRef *)typeRef;
    std::vector<std::unique_ptr<CTypeRef>> params;
    for (std::unique_ptr<TypeRef> &param : paramTypeRef->params) {
      std::unique_ptr<CTypeRef> cParam = scanTypeRef(param.get());
      if (!cParam) {
	return nullptr;
      }
      params.push_back(std::move(cParam));
    }
    return std::make_unique<CParamTypeRef>(paramTypeRef->loc, paramTypeRef->name,
					   paramTypeRef->hasReturnType, std::move(params));
  }
  default:
    error(typeRef->loc, "Internal (scanTypeRef)");
    return nullptr;
  }
}
