//========================================================================
//
// CodeGenModule.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "CodeGenModule.h"
#include "BytecodeFile.h"
#include "CodeGenFunc.h"
#include "Error.h"
#include "FileNames.h"
#include "Parser.h"
#include "SysIO.h"

//------------------------------------------------------------------------

bool codeGenModule(CModule *cmod, Context &ctx) {
  createDir(makeObjectDirPath(cmod->dir));

  std::string contents;
  if (!readFile(cmod->srcPath, contents)) {
    error(Location(), "Couldn't read source file '%s'", cmod->srcPath.c_str());
    return false;
  }
  Parser parser(contents, makeShortFileName(cmod->dir, cmod->name, cmod->isHeader));
  std::unique_ptr<Module> module = parser.parseModule();
  if (!module) {
    return false;
  }

  ctx.moduleBeingCompiled = cmod;
  ctx.nextDataLabel = 0;
  BytecodeFile bcFile(bytecodeError);
  bool ok = true;
  for (std::unique_ptr<ModuleElem> &elem : module->elems) {
    if (elem->kind() == ModuleElem::Kind::funcDefn) {
      ok &= codeGenFunc((FuncDefn *)elem.get(), ctx, bcFile);
    }
  }
  ctx.moduleBeingCompiled = nullptr;
  if (!ok) {
    return false;
  }

  std::string objFileName = makeObjectFileName(cmod->dir, cmod->name);
  return bcFile.write(objFileName);
}
