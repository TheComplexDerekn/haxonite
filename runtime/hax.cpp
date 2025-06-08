//========================================================================
//
// hax.cpp
//
// Wrapper program to compile and run a Haxonite program: this runs
// haxc and haxrun.
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
#include "SysIO.h"

int main(int argc, char *argv[]) {
  //--- parse args
  std::vector<std::string> paths;
  std::string configFile;
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
    } else if (!strcmp(argv[argIdx], "-v")) {
      verbose = true;
      ++argIdx;
    } else {
      ok = false;
    }
  }
  if (!ok || argc - argIdx < 1) {
    fprintf(stderr, "Usage: hax [-v] [-cfg <cfg-file>] [-path <dir> ...] <top-module> [arg ...]\n");
    exit(1);
  }
  char *topModuleName = argv[argIdx];

  //--- run haxc
  std::vector<std::string> cmd;
  cmd.push_back("haxc");
  for (const std::string &p : paths) {
    cmd.push_back("-path");
    cmd.push_back(p);
  }
  if (verbose) {
    cmd.push_back("-v");
  }
  cmd.push_back(topModuleName);
  int exitStatus;
  if (!run(cmd, exitStatus) || exitStatus != 0) {
    fprintf(stderr, "ERROR: compilation failed\n");
    exit(1);
  }

  //--- run haxrun
  cmd.clear();
  cmd.push_back("haxrun");
  for (const std::string &p : paths) {
    cmd.push_back("-path");
    cmd.push_back(p);
  }
  if (!configFile.empty()) {
    cmd.push_back("-cfg");
    cmd.push_back(configFile);
  }
  if (verbose) {
    cmd.push_back("-v");
  }
  cmd.push_back(topModuleName);
  for (int i = argIdx + 1; i < argc; ++i) {
    cmd.push_back(argv[i]);
  }
  if (!run(cmd, exitStatus)) {
    fprintf(stderr, "ERROR: run failed\n");
    exit(1);
  }
  return exitStatus;
}
