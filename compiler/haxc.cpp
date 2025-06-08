//========================================================================
//
// haxc.cpp
//
// Main module of the Haxonite compiler.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_set>
#include "BuiltinTypes.h"
#include "CodeGenModule.h"
#include "Context.h"
#include "FileNames.h"
#include "FunctionChecker.h"
#include "Instantiator.h"
#include "Link.h"
#include "ModuleScanner.h"
#include "SysIO.h"
#include "TypeRefConnector.h"

//------------------------------------------------------------------------

static bool needsCompiled(CModule *module, Context &ctx);
static bool olderThanSrc(DateTime timestamp, CModule *module);
static bool olderThanObj(DateTime timestamp, Context &ctx);

//------------------------------------------------------------------------

int main(int argc, char *argv[]) {
  bool verbose = false;
  std::vector<std::string> paths;
  bool ok = true;
  int argIdx = 1;
  while (argIdx < argc && argv[argIdx][0] == '-' && ok) {
    if (!strcmp(argv[argIdx], "-v")) {
      verbose = true;
      ++argIdx;
    } else if (!strcmp(argv[argIdx], "-path") && argIdx+1 < argc) {
      paths.push_back(argv[argIdx+1]);
      argIdx += 2;
    } else {
      ok = false;
    }
  }
  if (!ok || argIdx != argc - 1) {
    fprintf(stderr, "Usage: haxc [-v] [-path <dir> ...] <top-module>\n");
    exit(1);
  }
  char *topModuleName = argv[argIdx];

  Context ctx;
  if (!ctx.initSearchPath(paths)) {
    fprintf(stderr, "HAXONITEPATH is unset\n");
    exit(1);
  }
  ctx.verbose = verbose;

  if (verbose) {
    printf(">> Scanning builtin modules\n");
  }
  if (!scanBuiltinModule("haxonite", ctx) ||
      !scanBuiltinModule("gfx", ctx)) {
    if (verbose) {
      printf("** Builtin module scan failed\n");
    }
    exit(1);
  }

  if (!scanContainerTypeHeaders("Vector", "Set", "Map", ctx)) {
    if (verbose) {
      printf("** Container type header scan failed\n");
    }
    exit(1);
  }

  addBuiltinTypes(ctx);

  if (verbose) {
    printf(">> Scanning modules\n");
  }
  if (!scanModules(topModuleName, ctx)) {
    if (verbose) {
      printf("** Module scan failed\n");
    }
    exit(1);
  }

  if (verbose) {
    printf(">> Connecting type refs\n");
  }
  if (!connectTypeRefs(ctx)) {
    if (verbose) {
      printf("** Type ref connection failed\n");
    }
    exit(1);
  }

  if (verbose) {
    printf(">> Checking functions\n");
  }
  if (!checkFunctions(ctx)) {
    if (verbose) {
      printf("** Function check failed\n");
    }
    exit(1);
  }

  if (verbose) {
    printf(">> Instantiating parameterized types\n");
  }
  if (!instantiateContainerTypes(ctx)) {
    if (verbose) {
      printf("** Container type instantiation failed\n");
    }
    exit(1);
  }

  bool newObjFiles = false;
  ok = true;
  for (auto &pair : ctx.modules) {
    CModule *module = pair.second.get();
    if (needsCompiled(module, ctx)) {
      if (verbose) {
	printf(">> Compiling module %s\n", module->name.c_str());
      }
      ok &= codeGenModule(module, ctx);
      newObjFiles = true;
    } else {
      if (verbose) {
	printf(">> Module %s is up to date\n", module->name.c_str());
      }
    }
  }
  if (!ok) {
    if (verbose) {
      printf("** Module compilation failed\n");
    }
    exit(1);
  }

  std::string exePath = makeExecutableFileName(ctx.topModule->dir, ctx.topModule->name);
  if (!pathIsFile(exePath) ||
      newObjFiles ||
      olderThanObj(pathModTime(exePath), ctx)) {
    if (verbose) {
      printf(">> Linking executable\n");
    }
    if (!linkExecutable(ctx)) {
      exit(1);
    }
  } else {
    if (verbose) {
      printf(">> Executable is up to date\n");
    }
  }
}

// Return true if [module] needs to be (re)compiled.
static bool needsCompiled(CModule *module, Context &ctx) {
  // binary modules (represented by .haxh header files) are not
  // recompiled
  if (module->isHeader) {
    return false;
  }

  // check if obj file doesn't exist
  if (!module->objTimestamp.valid()) {
    return true;
  }

  // check if existing obj file is older than the source module or any
  // of its imports
  if (olderThanSrc(module->objTimestamp, module)) {
    return true;
  }

  return false;
}

// Return true if [timestamp] is older than the source file for
// [module] or any of its imports. [visitedModules] tracks the
// already-checked modules.
static bool olderThanSrc(DateTime timestamp, CModule *module) {
  if (timestamp.cmp(module->srcTimestamp) < 0) {
    return true;
  }
  for (CModule *import : module->imports) {
    if (timestamp.cmp(import->srcTimestamp) < 0) {
      return true;
    }
  }
  return false;
}

// Return true if [timestamp] is older than any module object file.
static bool olderThanObj(DateTime timestamp, Context &ctx) {
  for (auto &pair : ctx.modules) {
    CModule *module = pair.second.get();
    if (timestamp.cmp(module->objTimestamp) < 0) {
      return true;
    }
  }
  return false;
}
