//========================================================================
//
// bcdisasm.cpp
//
// Bytecode disassembler.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unordered_map>
#include <vector>
#include "BytecodeDefs.h"
#include "BytecodeFile.h"

static int32_t extractInt32(BytecodeFile &bcFile, uint32_t &bytecodeAddr);
static uint32_t extractUint32(BytecodeFile &bcFile, uint32_t &bytecodeAddr);
static int64_t extractInt56(BytecodeFile &bcFile, uint32_t &bytecodeAddr);
static uint64_t extractUint56(BytecodeFile &bcFile, uint32_t &bytecodeAddr);
static uint64_t extractUint64(BytecodeFile &bcFile, uint32_t &bytecodeAddr);
static float extractFloat32(BytecodeFile &bcFile, uint32_t &bytecodeAddr);
static void bcError(const std::string &msg);

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: bcasm <in.bc>\n");
    exit(1);
  }
  char *bcFileName = argv[1];

  BytecodeFile bcFile(bcError);
  if (!bcFile.read(bcFileName)) {
    fprintf(stderr, "Couldn't load bytecode file '%s'\n", bcFileName);
    exit(1);
  }

  //--- dump data
  std::unordered_map<uint32_t, uint32_t> dataLabelMap; // data addr -> label index
  std::unordered_map<uint32_t, uint32_t> dataRelocMap; // operand addr -> label index
  bcFile.forEachDataLabel([&](uint32_t idx, uint32_t dataAddr,
			      const std::vector<uint32_t> &instrAddrs) {
      dataLabelMap[dataAddr] = idx;
      for (uint32_t instrAddr : instrAddrs) {
	dataRelocMap[instrAddr] = idx;
      }
    });
  int inDirective = 0;
  for (uint32_t dataAddr = 0; dataAddr < bcFile.dataSectionLength(); ++dataAddr) {
    auto iter = dataLabelMap.find(dataAddr);
    if (iter != dataLabelMap.end()) {
      if (inDirective) {
	printf("\n");
	inDirective = false;
      }
      printf("@D%u:\n", iter->second);
    }
    if (!inDirective) {
      printf("[%04x] data.byte", dataAddr);
    }
    printf(" %02x", bcFile.dataSectionByte(dataAddr));
    ++inDirective;
    if (inDirective == 16) {
      printf("\n");
      inDirective = false;
    }
  }
  printf("\n");

  //--- dump bytecode
  std::unordered_map<uint32_t, std::string> funcDefnMap; // bytecode addr -> func name
  bcFile.forEachFuncDefn([&](const std::string &funcName, uint32_t bytecodeAddr) {
      funcDefnMap[bytecodeAddr] = funcName;
    });
  std::unordered_map<uint32_t, std::string> bytecodeRelocMap; // operand addr -> func name
  bcFile.forEachBytecodeReloc([&](const std::string &funcName,
				  const std::vector<uint32_t> &instrAddrs) {
      for (uint32_t instrAddr : instrAddrs) {
	bytecodeRelocMap[instrAddr] = funcName;
      }
    });
  std::unordered_map<uint32_t, std::string> nativeRelocMap; // operand addr -> func name
  bcFile.forEachNativeReloc([&](const std::string &funcName,
				  const std::vector<uint32_t> &instrAddrs) {
      for (uint32_t instrAddr : instrAddrs) {
	nativeRelocMap[instrAddr] = funcName;
      }
    });
  uint32_t bytecodeAddr = 0;
  while (bytecodeAddr < bcFile.bytecodeSectionLength()) {
    auto iter = funcDefnMap.find(bytecodeAddr);
    if (iter != funcDefnMap.end()) {
      printf("*%s:\n", iter->second.c_str());
    }
    printf("[%04x] ", bytecodeAddr);
    uint8_t opcode = bcFile.bytecodeSectionByte(bytecodeAddr++);
    printf("%s", bcOpcodeToStringMap[opcode]);
    if (opcode == bcOpcodePushI) {
      int64_t immed = extractInt56(bcFile, bytecodeAddr);
      printf(" %" PRId64, immed);
    } else if (opcode == bcOpcodePushF) {
      float immed = extractFloat32(bcFile, bytecodeAddr);
      printf(" %f", immed);
    } else if (opcode == bcOpcodePushBcode) {
      auto iter = bytecodeRelocMap.find(bytecodeAddr);
      uint64_t offset = extractUint56(bcFile, bytecodeAddr);
      if (iter == bytecodeRelocMap.end()) {
	printf(" {0x%" PRIx64 "}", offset);
      } else {
	printf(" %s", iter->second.c_str());
      }
    } else if (opcode == bcOpcodePushData) {
      auto iter = dataRelocMap.find(bytecodeAddr);
      uint64_t offset = extractUint64(bcFile, bytecodeAddr);
      if (iter == dataRelocMap.end()) {
	printf(" {0x%" PRIx64 "}", offset);
      } else {
	printf(" D%u", iter->second);
      }
    } else if (opcode == bcOpcodePushNative) {
      auto iter = nativeRelocMap.find(bytecodeAddr);
      uint64_t offset = extractUint64(bcFile, bytecodeAddr);
      if (iter == nativeRelocMap.end()) {
	printf(" {0x%" PRIx64 "}", offset);
      } else {
	printf(" %s", iter->second.c_str());
      }
    } else if (opcode == bcOpcodeGetStack) {
      uint32_t idx = extractUint32(bcFile, bytecodeAddr);
      printf(" %u", idx);
    } else if (opcode == bcOpcodeBranchTrue ||
	       opcode == bcOpcodeBranchFalse ||
	       opcode == bcOpcodeBranch) {
      int32_t relOffset = extractInt32(bcFile, bytecodeAddr);
      uint32_t offset = (uint32_t)(bytecodeAddr + relOffset);
      printf(" 0x%04x", offset);
    }
    printf("\n");
  }
}

static int32_t extractInt32(BytecodeFile &bcFile, uint32_t &bytecodeAddr) {
  if (bytecodeAddr + 4 > bcFile.bytecodeSectionLength()) {
    fprintf(stderr, "Bounds error\n");
    exit(1);
  }
  int32_t x = (bcFile.bytecodeSectionByte(bytecodeAddr    )      ) |
              (bcFile.bytecodeSectionByte(bytecodeAddr + 1) <<  8) |
              (bcFile.bytecodeSectionByte(bytecodeAddr + 2) << 16) |
              (bcFile.bytecodeSectionByte(bytecodeAddr + 3) << 24);
  bytecodeAddr += 4;
  return x;
}

static uint32_t extractUint32(BytecodeFile &bcFile, uint32_t &bytecodeAddr) {
  if (bytecodeAddr + 4 > bcFile.bytecodeSectionLength()) {
    fprintf(stderr, "Bounds error\n");
    exit(1);
  }
  uint32_t x = (bcFile.bytecodeSectionByte(bytecodeAddr    )      ) |
               (bcFile.bytecodeSectionByte(bytecodeAddr + 1) <<  8) |
               (bcFile.bytecodeSectionByte(bytecodeAddr + 2) << 16) |
               (bcFile.bytecodeSectionByte(bytecodeAddr + 3) << 24);
  bytecodeAddr += 4;
  return x;
}

static int64_t extractInt56(BytecodeFile &bcFile, uint32_t &bytecodeAddr) {
  if (bytecodeAddr + 7 > bcFile.bytecodeSectionLength()) {
    fprintf(stderr, "Bounds error\n");
    exit(1);
  }
  int64_t x = ((int64_t)bcFile.bytecodeSectionByte(bytecodeAddr    )      ) |
              ((int64_t)bcFile.bytecodeSectionByte(bytecodeAddr + 1) <<  8) |
              ((int64_t)bcFile.bytecodeSectionByte(bytecodeAddr + 2) << 16) |
              ((int64_t)bcFile.bytecodeSectionByte(bytecodeAddr + 3) << 24) |
              ((int64_t)bcFile.bytecodeSectionByte(bytecodeAddr + 4) << 32) |
              ((int64_t)bcFile.bytecodeSectionByte(bytecodeAddr + 5) << 40) |
              ((int64_t)bcFile.bytecodeSectionByte(bytecodeAddr + 6) << 48);
  if (x & ((int64_t)1 << 55)) {
    x |= (int64_t)0xff << 56;
  }
  bytecodeAddr += 7;
  return x;
}

static uint64_t extractUint56(BytecodeFile &bcFile, uint32_t &bytecodeAddr) {
  if (bytecodeAddr + 7 > bcFile.bytecodeSectionLength()) {
    fprintf(stderr, "Bounds error\n");
    exit(1);
  }
  uint64_t x = ((uint64_t)bcFile.bytecodeSectionByte(bytecodeAddr    )      ) |
               ((uint64_t)bcFile.bytecodeSectionByte(bytecodeAddr + 1) <<  8) |
               ((uint64_t)bcFile.bytecodeSectionByte(bytecodeAddr + 2) << 16) |
               ((uint64_t)bcFile.bytecodeSectionByte(bytecodeAddr + 3) << 24) |
               ((uint64_t)bcFile.bytecodeSectionByte(bytecodeAddr + 4) << 32) |
               ((uint64_t)bcFile.bytecodeSectionByte(bytecodeAddr + 5) << 40) |
               ((uint64_t)bcFile.bytecodeSectionByte(bytecodeAddr + 6) << 48);
  bytecodeAddr += 7;
  return x;
}

static uint64_t extractUint64(BytecodeFile &bcFile, uint32_t &bytecodeAddr) {
  if (bytecodeAddr + 8 > bcFile.bytecodeSectionLength()) {
    fprintf(stderr, "Bounds error\n");
    exit(1);
  }
  uint64_t x = ((uint64_t)bcFile.bytecodeSectionByte(bytecodeAddr    )      ) |
               ((uint64_t)bcFile.bytecodeSectionByte(bytecodeAddr + 1) <<  8) |
               ((uint64_t)bcFile.bytecodeSectionByte(bytecodeAddr + 2) << 16) |
               ((uint64_t)bcFile.bytecodeSectionByte(bytecodeAddr + 3) << 24) |
               ((uint64_t)bcFile.bytecodeSectionByte(bytecodeAddr + 4) << 32) |
               ((uint64_t)bcFile.bytecodeSectionByte(bytecodeAddr + 5) << 40) |
               ((uint64_t)bcFile.bytecodeSectionByte(bytecodeAddr + 5) << 48) |
               ((uint64_t)bcFile.bytecodeSectionByte(bytecodeAddr + 6) << 56);
  bytecodeAddr += 8;
  return x;
}

static float extractFloat32(BytecodeFile &bcFile, uint32_t &bytecodeAddr) {
  if (bytecodeAddr + 4 > bcFile.bytecodeSectionLength()) {
    fprintf(stderr, "Bounds error\n");
    exit(1);
  }
  union {
    uint32_t i;
    float f;
  } x;
  x.i = ((uint32_t)bcFile.bytecodeSectionByte(bytecodeAddr    )      ) |
        ((uint32_t)bcFile.bytecodeSectionByte(bytecodeAddr + 1) <<  8) |
        ((uint32_t)bcFile.bytecodeSectionByte(bytecodeAddr + 2) << 16) |
        ((uint32_t)bcFile.bytecodeSectionByte(bytecodeAddr + 3) << 24);
  bytecodeAddr += 4;
  return x.f;
}

static void bcError(const std::string &msg) {
  fprintf(stderr, "BC ERROR: %s\n", msg.c_str());
}
