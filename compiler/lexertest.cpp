// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "Lexer.h"

static std::string readFile(FILE *in) {
  std::string s;
  char buf[4096];
  while (fgets(buf, sizeof(buf), in)) {
    s.append(buf);
  }
  return s;
}

int main() {
  std::string input = readFile(stdin);
  Lexer lexer(input, "stdin");
  while (true) {
    Token tok = lexer.get(0);
    if (tok.is(Token::Kind::eof)) {
      break;
    }
    printf("%s: '%s'\n", tok.kindName().c_str(), tok.str().c_str());
    lexer.shift();
  }
}
