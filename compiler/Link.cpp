//========================================================================
//
// Link.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "Link.h"
#include "BytecodeFile.h"
#include "Error.h"
#include "FileNames.h"
#include "SysIO.h"

//------------------------------------------------------------------------

bool linkExecutable(Context &ctx) {
  createDir(makeBinDirPath(ctx.topModule->dir));
  std::string exePath = makeExecutableFileName(ctx.topModule->dir, ctx.topModule->name);

  BytecodeFile bcFile(bytecodeError);

  bool ok = true;
  for (auto &pair : ctx.modules) {
    CModule *cmod = pair.second.get();
    if (cmod->isNative) {
      continue;
    }
    BytecodeFile bcModule(bytecodeError);
    if (!bcModule.read(cmod->objPath)) {
      ok = false;
      continue;
    }
    bcFile.appendBytecodeFile(bcModule);
  }

  return ok && bcFile.resolveRelocs()
            && bcFile.write(exePath);
}
