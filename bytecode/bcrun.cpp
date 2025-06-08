//========================================================================
//
// bcrun.cpp
//
// Simple bytecode interpreter.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include <stdio.h>
#include <stdlib.h>
#include "BytecodeEngine.h"

static void nativePrintString(BytecodeEngine &engine);
static void nativePrintInt(BytecodeEngine &engine);

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: bcrun <in.bc>\n");
    exit(1);
  }
  char *bcFileName = argv[1];

  BytecodeEngine engine("", 1024*1024, 1024*1024, false);
  engine.addNativeFunction("print_S", &nativePrintString);
  engine.addNativeFunction("print_I", &nativePrintInt);

  if (!engine.loadBytecodeFile(bcFileName)) {
    fprintf(stderr, "Failed to load bytecode file '%s'\n", bcFileName);
    exit(1);
  }

  if (!engine.callFunction("main", 0)) {
    fprintf(stderr, "No 'main' function\n");
    exit(1);
  }

  return 0;
}

static NativeFuncDefn(nativePrintString) {
  if (engine.nArgs() == 1) {
    Cell &arg = engine.arg(0);
    if (cellIsPtr(arg)) {
      // string consists of a 7-byte length + 1-byte tag + data
      uint8_t *s = (uint8_t *)cellPtr(arg);
      int64_t length =  (int64_t)s[0] |
                       ((int64_t)s[1] <<  8) |
                       ((int64_t)s[2] << 16) |
                       ((int64_t)s[3] << 24) |
                       ((int64_t)s[4] << 32) |
                       ((int64_t)s[5] << 40) |
                       ((int64_t)s[6] << 48);
      printf("%.*s", (int)length, s + 8);
    } else {
      printf("Called native 'print(String)' function with wrong arg type\n");
    }
  } else {
    printf("Called native 'print(String)' function with wrong number (%d) of args\n",
	   engine.nArgs());
  }
  engine.push(cellMakeInt(0));
}

static NativeFuncDefn(nativePrintInt) {
  if (engine.nArgs() == 1) {
    Cell &arg = engine.arg(0);
    if (cellIsInt(arg)) {
      printf("%ld", cellInt(arg));
    } else {
      printf("Called native 'print(Int)' function with wrong arg type\n");
    }
  } else {
    printf("Called native 'print(Int)' function with wrong number (%d) of args\n",
	   engine.nArgs());
  }
  engine.push(cellMakeInt(0));
}
