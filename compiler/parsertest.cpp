// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include "AST.h"
#include "Parser.h"

static std::string readFile(FILE *in) {
  std::string s;
  char buf[4096];
  while (fgets(buf, sizeof(buf), in)) {
    s.append(buf);
  }
  return s;
}

int main(int argc, char *argv[]) {
  bool header = false;
  int i = 1;
  while (i < argc) {
    if (!strcmp(argv[i], "-header")) {
      header = true;
      ++i;
    } else {
      break;
    }
  }
  if (i < argc) {
    fprintf(stderr, "Usage: parsertest [-header] <in.hax/haxh\n");
    exit(1);
  }

  std::string input = readFile(stdin);
  Parser parser(input, "stdin");
  std::unique_ptr<Module> module = header ? parser.parseHeader() : parser.parseModule();
  if (!module) {
    fprintf(stderr, "Parse failed\n");
    exit(1);
  }
  printf("%s", module->toString().c_str());
}
