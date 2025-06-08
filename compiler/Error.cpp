//========================================================================
//
// Error.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include <stdarg.h>
#include <stdio.h>
#include "Error.h"

void error(Location loc, const char *fmt, ...) {
  if (loc.hasPath()) {
    if (loc.hasLine()) {
      fprintf(stderr, "Error [%s:%d]: ", loc.path().c_str(), loc.line());
    } else {
      fprintf(stderr, "Error [%s]: ", loc.path().c_str());
    }
  } else {
    fprintf(stderr, "Error: ");
  }
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n");
}

void bytecodeError(const std::string &msg) {
  fprintf(stderr, "Bytecode error: %s\n", msg.c_str());
}
