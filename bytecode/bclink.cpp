//========================================================================
//
// bclink.cpp
//
// Bytecode linker.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "BytecodeFile.h"

static void error(const std::string &msg);

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "Usage: bclink <output.bce> <input.bc> <input.bc> ...\n");
    exit(1);
  }
  char *exeFileName = argv[1];
  int firstObjIdx = 2;
  int lastObjIdx = argc;

  BytecodeFile bcFile(error);

  bool ok = true;
  for (int i = firstObjIdx; i < lastObjIdx; ++i) {
    BytecodeFile bcModule(error);
    if (bcModule.read(argv[i])) {
      bcFile.appendBytecodeFile(bcModule);
    } else {
      ok = false;
    }
  }

  ok = ok && bcFile.resolveRelocs()
          && bcFile.write(exeFileName);

  exit(ok ? 0 : 1);
}

static void error(const std::string &msg) {
  fprintf(stderr, "ERROR: %s\n", msg.c_str());
}

