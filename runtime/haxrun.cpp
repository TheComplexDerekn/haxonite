//========================================================================
//
// bcrun.cpp
//
// Bytecode interpreter for Haxonite programs.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "BytecodeEngine.h"
#include "SysIO.h"
#include "runtime_alloc.h"
#include "runtime_datetime.h"
#include "runtime_File.h"
#include "runtime_format.h"
#include "runtime_gfx.h"
#include "runtime_Map.h"
#include "runtime_math.h"
#include "runtime_random.h"
#include "runtime_regex.h"
#include "runtime_serdeser.h"
#include "runtime_Set.h"
#include "runtime_String.h"
#include "runtime_StringBuf.h"
#include "runtime_system.h"
#include "runtime_Vector.h"

#define defaultStackSize (1024 * 1024)
#define defaultInitialHeapSize (1024 * 1024)

static void setupNativeFuncs(BytecodeEngine &engine);
static bool findExecutable(const std::string &topModuleName,
			   const std::vector<std::string> &paths,
			   std::string &exePath);

int main(int argc, char *argv[]) {
  std::vector<std::string> paths;
  std::string configFile;
  size_t stackSize = defaultStackSize;
  size_t initialHeapSize = defaultInitialHeapSize;
  bool verbose = false;
  bool ok = true;
  int argIdx = 1;
  while (argIdx < argc && argv[argIdx][0] == '-' && ok) {
    if (!strcmp(argv[argIdx], "-path") && argIdx+1 < argc) {
      paths.push_back(argv[argIdx+1]);
      argIdx += 2;
    } else if (!strcmp(argv[argIdx], "-cfg") && argIdx+1 < argc) {
      configFile = argv[argIdx+1];
      argIdx += 2;
    } else if (!strcmp(argv[argIdx], "-stack") && argIdx+1 < argc) {
      stackSize = atol(argv[argIdx+1]);
      argIdx += 2;
    } else if (!strcmp(argv[argIdx], "-heap") && argIdx+1 < argc) {
      initialHeapSize = atol(argv[argIdx+1]);
      argIdx += 2;
    } else if (!strcmp(argv[argIdx], "-v")) {
      verbose = true;
      ++argIdx;
    } else {
      ok = false;
    }
  }
  if (!ok || argc - argIdx < 1) {
    fprintf(stderr, "Usage: haxrun [-v] [-path <dir> ...] [-cfg <cfg-file>] [-stack <size>] [-heap <size>] <top-module> [arg ...]\n");
    exit(1);
  }
  char *topModuleName = argv[argIdx];

  BytecodeEngine engine(configFile, stackSize, initialHeapSize, verbose);
  setupNativeFuncs(engine);

  std::string exePath;
  if (!findExecutable(topModuleName, paths, exePath)) {
    fprintf(stderr, "ERROR: Couldn't find an executable for '%s' on HAXONITEPATH\n",
	    topModuleName);
    exit(1);
  }
  
  if (!engine.loadBytecodeFile(exePath)) {
    fprintf(stderr, "ERROR: Failed to load bytecode file '%s'\n", exePath.c_str());
    exit(1);
  }

  setCommandLineArgs(argc - (argIdx+1), argv + (argIdx+1), engine);

  if (!engine.callFunction("main", 0)) {
    fprintf(stderr, "ERROR: No 'main' function in '%s'\n", exePath.c_str());
    exit(1);
  }

  return 0;
}

static void setupNativeFuncs(BytecodeEngine &engine) {
  runtime_alloc_init(engine);
  runtime_datetime_init(engine);
  runtime_File_init(engine);
  runtime_format_init(engine);
  runtime_gfx_init(engine);
  runtime_Map_init(engine);
  runtime_math_init(engine);
  runtime_random_init(engine);
  runtime_regex_init(engine);
  runtime_serdeser_init(engine);
  runtime_Set_init(engine);
  runtime_String_init(engine);
  runtime_StringBuf_init(engine);
  runtime_system_init(engine);
  runtime_Vector_init(engine);
}

static bool findExecutable(const std::string &topModuleName,
			   const std::vector<std::string> &paths,
			   std::string &exePath) {
  std::vector<std::string> searchPath;
  for (const std::string &p : paths) {
    searchPath.push_back(p);
  }
  std::string s = getEnvVar("HAXONITEPATH");
  size_t i = 0;
  while (i < s.size()) {
    size_t j = s.find(':', i);
    if (j == std::string::npos) {
      searchPath.push_back(s.substr(i));
      break;
    }
    searchPath.push_back(s.substr(i, j - i));
    i = j + 1;
  }

  for (const std::string &dir : searchPath) {
    std::string path = dir + "/bin/" + topModuleName + ".haxe";
    if (pathIsFile(path)) {
      exePath = path;
      return true;
    }
  }
  return false;
}
