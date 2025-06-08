//========================================================================
//
// bcasm.cpp
//
// Bytecode assembler.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unordered_map>
#include "BytecodeDefs.h"
#include "BytecodeFile.h"

static int errorCount = 0;

static bool tokenizeLine(char *line, int lineNum, std::vector<std::string> &tokens);
static void bcError(const std::string &msg);
static void error(int lineNum, const char *fmt, ...);

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: bcasm <in.bcasm> <out.bc>\n");
    exit(1);
  }
  char *asmFileName = argv[1];
  char *bcFileName = argv[2];

  FILE *in = fopen(asmFileName, "r");
  if (!in) {
    fprintf(stderr, "Couldn't open assembly file '%s'\n", asmFileName);
    exit(1);
  }

  BytecodeFile bcFile(bcError);
  std::unordered_map<std::string, uint32_t> dataLabels;
  std::unordered_map<std::string, uint32_t> codeLabels;

  int lineNum = 0;
  char line[500];
  while (fgets(line, sizeof(line), in)) {
    ++lineNum;
    std::vector<std::string> tokens;
    if (!tokenizeLine(line, lineNum, tokens) || tokens.empty()) {
      continue;
    }
    if (tokens[0][0] == '*' && tokens[0][tokens[0].size() - 1] == ':') {
      std::string label = tokens[0].substr(1, tokens[0].size() - 2);
      bcFile.setFunc(label);
    } else if (tokens[0][0] == '@' && tokens[0][tokens[0].size() - 1] == ':') {
      std::string label = tokens[0].substr(1, tokens[0].size() - 2);
      dataLabels[label] = bcFile.allocAndSetDataLabel();
    } else if (tokens[0][tokens[0].size() - 1] == ':') {
      std::string label = tokens[0].substr(0, tokens[0].size() - 1);
      if (codeLabels.find(label) == codeLabels.end()) {
	codeLabels[label] = bcFile.allocCodeLabel();
      }
      bcFile.setCodeLabel(codeLabels[label]);
    } else if (tokens[0] == "data.byte") {
      if (tokens.size() < 2) {
	error(lineNum, "The 'data.byte' directive requires at least one operand");
	continue;
      }
      std::vector<uint8_t> data;
      for (size_t i = 1; i < tokens.size(); ++i) {
	data.push_back((uint8_t)std::stoi(tokens[i], nullptr, 16));
      }
      bcFile.addData(&data[0], data.size());
    } else if (tokens[0] == "data.string") {
      if (tokens.size() != 2) {
	error(lineNum, "The 'data.string' directive takes one operand");
	continue;
      }
      bcFile.addData((uint8_t *)tokens[1].c_str(), tokens[1].size());
    } else if (tokens[0] == "data.align") {
      bcFile.alignData();
    } else {
      auto iter = bcStringToOpcodeMap.find(tokens[0]);
      if (iter == bcStringToOpcodeMap.end()) {
	error(lineNum, "Unknown opcode '%s'", tokens[0].c_str());
	continue;
      }
      uint8_t opcode = iter->second;
      if (opcode == bcOpcodePushI) {
	if (tokens.size() != 2) {
	  error(lineNum, "The 'push.i' instruction takes one operand");
	  continue;
	}
	int64_t immed = (int64_t)std::stoll(tokens[1]);
	bcFile.addPushIInstr(immed);
      } else if (opcode == bcOpcodePushF) {
	if (tokens.size() != 2) {
	  error(lineNum, "The 'push.f' instruction takes one operand");
	  continue;
	}
	float immed = std::stof(tokens[1]);
	bcFile.addPushFInstr(immed);
      } else if (opcode == bcOpcodePushBcode) {
	if (tokens.size() != 2) {
	  error(lineNum, "The 'push.bcode' instruction takes one operand");
	  continue;
	}
	bcFile.addPushBcodeInstr(tokens[1]);
      } else if (opcode == bcOpcodePushData) {
	if (tokens.size() != 2) {
	  error(lineNum, "The 'push.data' instruction takes one operand");
	  continue;
	}
	if (dataLabels.find(tokens[1]) == dataLabels.end()) {
	  error(lineNum, "Undefined data label in 'push.data' instruction");
	  continue;
	}
	bcFile.addPushDataInstr(dataLabels[tokens[1]]);
      } else if (opcode == bcOpcodePushNative) {
	if (tokens.size() != 2) {
	  error(lineNum, "The 'push.native' instruction takes one operand");
	  continue;
	}
	bcFile.addPushNativeInstr(tokens[1]);
      } else if (opcode == bcOpcodeBranchTrue ||
		 opcode == bcOpcodeBranchFalse ||
		 opcode == bcOpcodeBranch) {
	if (tokens.size() != 2) {
	  error(lineNum, "The '%s' instruction takes one operand", tokens[0].c_str());
	  continue;
	}
	if (codeLabels.find(tokens[1]) == codeLabels.end()) {
	  codeLabels[tokens[1]] = bcFile.allocCodeLabel();
	}
	bcFile.addBranchInstr(opcode, codeLabels[tokens[1]]);
      } else {
	if (tokens.size() != 1) {
	  error(lineNum, "The '%s' instruction takes zero operands", tokens[0].c_str());
	  continue;
	}
	bcFile.addInstr(opcode);
      }
    }
  }

  fclose(in);

  if (errorCount) {
    exit(1);
  }

  if (!bcFile.write(bcFileName)) {
    fprintf(stderr, "Couldn't write bytecode file '%s'\n", bcFileName);
    exit(1);
  }

  return 0;
}

static bool tokenizeLine(char *line, int lineNum, std::vector<std::string> &tokens) {
  char *p0 = line;
  while (true) {
    // skip whitespace
    while (*p0 == ' ' || *p0 == '\t' || *p0 == '\n') {
      ++p0;
    }
    if (!*p0 || *p0 == ';') {
      break;
    }

    // get quoted string
    if (*p0 == '"') {
      ++p0;
      std::string token;
      while (*p0 != '"') {
	char c = *p0;
	if (c == '\0' || c == '\n') {
	  error(lineNum, "Unterminated string");
	  return false;
	}
	++p0;
	if (c == '\\') {
	  c = *p0;
	  if (c == '\0' || c == '\n') {
	    error(lineNum, "Unterminated string");
	    return false;
	  }
	  ++p0;
	  switch (c) {
	  case 'n':
	    c = '\n';
	    break;
	  case 'r':
	    c = '\r';
	    break;
	  case 't':
	    c = '\t';
	    break;
	  case '"':
	  case '\\':
	    break;
	  default:
	    error(lineNum, "Invalid escape character in string");
	    return false;
	  }
	}
	token.push_back(c);
      }
      tokens.push_back(token);
      ++p0;

    // get regular token
    } else {
      char *p1;
      for (p1 = p0 + 1; *p1 && *p1 != ' ' && *p1 != '\t' && *p1 != '\n' && *p1 != ';'; ++p1) ;
      tokens.push_back(std::string(p0, p1 - p0));
      p0 = p1;
    }
  }

  return true;
}

static void bcError(const std::string &msg) {
  fprintf(stderr, "BC ERROR: %s\n", msg.c_str());
}

static void error(int lineNum, const char *fmt, ...) {
  va_list args;
  fprintf(stderr, "ERROR [%d]: ");
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n");
  ++errorCount;
}
