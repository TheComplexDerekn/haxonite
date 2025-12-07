//========================================================================
//
// BytecodeFile.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "BytecodeFile.h"
#include <string.h>
#include "BytecodeDefs.h"

//------------------------------------------------------------------------

static const char *headerString = "haxonite\x01\x00\x00\x00";

//------------------------------------------------------------------------
// read
//------------------------------------------------------------------------

bool BytecodeFile::read(const std::string &path) {
  clear();

  FILE *in = fopen(path.c_str(), "rb");
  if (!in) {
    return false;
  }

  bool ok = readHeader(in) &&
            readBytecodeSection(in) &&
            readDataSection(in) &&
            readFuncDefns(in) &&
            readBytecodeRelocs(in) &&
            readNativeRelocs(in) &&
            readDataLabels(in);

  fclose(in);
  return ok;
}

bool BytecodeFile::readHeader(FILE *in) {
  char header[12];
  return readBytes(header, 12, in) &&
         !memcmp(header, headerString, 12);
}

bool BytecodeFile::readBytecodeSection(FILE *in) {
  uint32_t length;
  if (!readBytes(&length, 4, in)) {
    return false;
  }
  bytecodeSection.resize(length);
  if (length && !readBytes(&bytecodeSection[0], length, in)) {
    return false;
  }
  return true;
}

bool BytecodeFile::readDataSection(FILE *in) {
  uint32_t length;
  if (!readBytes(&length, 4, in)) {
    return false;
  }
  dataSection.resize(length);
  if (length && !readBytes(&dataSection[0], length, in)) {
    return false;
  }
  return true;
}

bool BytecodeFile::readFuncDefns(FILE *in) {
  uint32_t nDefns;
  if (!readBytes(&nDefns, 4, in)) {
    return false;
  }
  bool ok = true;
  for (uint32_t i = 0; i < nDefns; ++i) {
    std::string name;
    if (!readName(name, in)) {
      return false;
    }
    uint32_t addr;
    if (!readBytes(&addr, 4, in)) {
      return false;
    }
    if (addr >= bytecodeSection.size()) {
      (*errorFunc)("Function defn for '" + name + "' has an invalid address");
      ok = false;
    } else {
      funcDefns[name] = addr;
    }
  }
  return ok;
}

bool BytecodeFile::readBytecodeRelocs(FILE *in) {
  uint32_t nRelocs;
  if (!readBytes(&nRelocs, 4, in)) {
    return false;
  }
  bool ok = true;
  for (uint32_t i = 0; i < nRelocs; ++i) {
    std::string name;
    if (!readName(name, in)) {
      return false;
    }
    uint32_t nInstrs;
    if (!readBytes(&nInstrs, 4, in)) {
      return false;
    }
    for (uint32_t j = 0; j < nInstrs; ++j) {
      uint32_t instrAddr;
      if (!readBytes(&instrAddr, 4, in)) {
	return false;
      }
      if (instrAddr >= bytecodeSection.size()) {
	(*errorFunc)("Bytecode reloc for '" + name + "' contains an invalid instruction address");
	ok = false;
      } else {
	bytecodeRelocs[name].push_back(instrAddr);
      }
    }
  }
  return ok;
}

bool BytecodeFile::readNativeRelocs(FILE *in) {
  uint32_t nRelocs;
  if (!readBytes(&nRelocs, 4, in)) {
    return false;
  }
  bool ok = true;
  for (uint32_t i = 0; i < nRelocs; ++i) {
    std::string name;
    if (!readName(name, in)) {
      return false;
    }
    uint32_t nInstrs;
    if (!readBytes(&nInstrs, 4, in)) {
      return false;
    }
    for (uint32_t j = 0; j < nInstrs; ++j) {
      uint32_t instrAddr;
      if (!readBytes(&instrAddr, 4, in)) {
	return false;
      }
      if (instrAddr >= bytecodeSection.size()) {
	(*errorFunc)("Native reloc for '" + name + "' contains an invalid instruction address");
	ok = false;
      } else {
	nativeRelocs[name].push_back(instrAddr);
      }
    }
  }
  return ok;
}

bool BytecodeFile::readDataLabels(FILE *in) {
  uint32_t nLabels;
  if (!readBytes(&nLabels, 4, in)) {
    return false;
  }
  bool ok = true;
  for (uint32_t i = 0; i < nLabels; ++i) {
    uint32_t dataAddr;
    if (!readBytes(&dataAddr, 4, in)) {
      return false;
    }
    if (dataAddr >= dataSection.size()) {
      (*errorFunc)("Data label #" + std::to_string(i) + " has an invalid address");
      ok = false;
    } else {
      dataLabels.emplace_back(dataAddr);
      uint32_t nInstrs;
      if (!readBytes(&nInstrs, 4, in)) {
	return false;
      }
      for (uint32_t j = 0; j < nInstrs; ++j) {
	uint32_t instrAddr;
	if (!readBytes(&instrAddr, 4, in)) {
	  return false;
	}
	if (instrAddr >= bytecodeSection.size()) {
	  (*errorFunc)("Data label #" + std::to_string(i) +
		       " contains an invalid instruction address");
	  ok = false;
	} else {
	  dataLabels.back().instrAddrs.push_back(instrAddr);
	}
      }
    }
  }
  return ok;
}

bool BytecodeFile::readName(std::string &name, FILE *in) {
  uint32_t length;
  if (!readBytes(&length, 4, in)) {
    return false;
  }
  name.resize(length);
  return readBytes(&name[0], length, in);
}

bool BytecodeFile::readBytes(void *buf, size_t nBytes, FILE *in) {
  return fread((char *)buf, 1, nBytes, in) == nBytes;
}

//------------------------------------------------------------------------
// write
//------------------------------------------------------------------------

bool BytecodeFile::write(const std::string &path) {
  if (!resolveCodeLabels()) {
    return false;
  }
  FILE *out = fopen(path.c_str(), "wb");
  if (!out) {
    return false;
  }
  writeHeader(out);
  writeBytecodeSection(out);
  writeDataSection(out);
  writeFuncDefns(out);
  writeBytecodeRelocs(out);
  writeNativeRelocs(out);
  writeDataLabels(out);
  fclose(out);
  return true;
}

void BytecodeFile::writeHeader(FILE *out) {
  fwrite(headerString, 1, 12, out);
}

void BytecodeFile::writeBytecodeSection(FILE *out) {
  uint32_t length = (uint32_t)bytecodeSection.size();
  fwrite((char *)&length, 1, 4, out);
  if (!bytecodeSection.empty()) {
    fwrite((char *)&bytecodeSection[0], 1, length, out);
  }
}

void BytecodeFile::writeDataSection(FILE *out) {
  uint32_t length = (uint32_t)dataSection.size();
  fwrite((char *)&length, 1, 4, out);
  if (!dataSection.empty()) {
    fwrite((char *)&dataSection[0], 1, length, out);
  }
}

void BytecodeFile::writeFuncDefns(FILE *out) {
  uint32_t n = (uint32_t)funcDefns.size();
  fwrite((char *)&n, 1, 4, out);
  for (auto &pair : funcDefns) {
    writeName(pair.first, out);
    fwrite((char *)&pair.second, 1, 4, out);
  }
}

void BytecodeFile::writeBytecodeRelocs(FILE *out) {
  uint32_t n = (uint32_t)bytecodeRelocs.size();
  fwrite((char *)&n, 1, 4, out);
  for (auto &pair : bytecodeRelocs) {
    writeName(pair.first, out);
    n = (uint32_t)pair.second.size();
    fwrite((char *)&n, 1, 4, out);
    for (uint32_t instrAddr : pair.second) {
      fwrite((char *)&instrAddr, 1, 4, out);
    }
  }
}

void BytecodeFile::writeNativeRelocs(FILE *out) {
  uint32_t n = (uint32_t)nativeRelocs.size();
  fwrite((char *)&n, 1, 4, out);
  for (auto &pair : nativeRelocs) {
    writeName(pair.first, out);
    n = (uint32_t)pair.second.size();
    fwrite((char *)&n, 1, 4, out);
    for (uint32_t offset : pair.second) {
      fwrite((char *)&offset, 1, 4, out);
    }
  }
}

void BytecodeFile::writeDataLabels(FILE *out) {
  uint32_t n = (uint32_t)dataLabels.size();
  fwrite((char *)&n, 1, 4, out);
  for (DataLabel &dataLabel : dataLabels) {
    fwrite((char *)&dataLabel.dataAddr, 1, 4, out);
    n = (uint32_t)dataLabel.instrAddrs.size();
    fwrite((char *)&n, 1, 4, out);
    for (uint32_t instrAddr : dataLabel.instrAddrs) {
      fwrite((char *)&instrAddr, 1, 4, out);
    }
  }
}

void BytecodeFile::writeName(const std::string &name, FILE *out) {
  uint32_t length = (uint32_t)name.size();
  fwrite((char *)&length, 1, 4, out);
  fwrite(name.c_str(), 1, length, out);
}

//------------------------------------------------------------------------
// code label resolution
//------------------------------------------------------------------------

// Resolve code labels: compute branch offsets and store them in the
// branch instructions.
bool BytecodeFile::resolveCodeLabels() {
  bool ok = true;
  for (CodeLabel &codeLabel : codeLabels) {
    if (!codeLabel.bytecodeAddrSet) {
      (*errorFunc)("Unresolved code label");
      ok = false;
      continue;
    }
    for (uint32_t instrAddr : codeLabel.instrAddrs) {
      if (instrAddr + 4 > bytecodeSection.size()) {
	(*errorFunc)("Invalid instruction address for code label");
	ok = false;
      } else {
	int64_t offset = (int64_t)codeLabel.bytecodeAddr - ((int64_t)instrAddr + 4);
	if (offset > INT64_C(0x7fffffff) || offset < -INT64_C(0x80000000)) {
	  (*errorFunc)("Branch target too far away in code label");
	  ok = false;
	} else {
	  uint8_t *p = (uint8_t *)&offset;
	  bytecodeSection[instrAddr    ] = p[0];
	  bytecodeSection[instrAddr + 1] = p[1];
	  bytecodeSection[instrAddr + 2] = p[2];
	  bytecodeSection[instrAddr + 3] = p[3];
	}
      }
    }
  }
  return ok;
}

//------------------------------------------------------------------------
// clear
//------------------------------------------------------------------------

void BytecodeFile::clear() {
  bytecodeSection.clear();
  dataSection.clear();
  funcDefns.clear();
  bytecodeRelocs.clear();
  nativeRelocs.clear();
  dataLabels.clear();
}

//------------------------------------------------------------------------
// append
//------------------------------------------------------------------------

bool BytecodeFile::appendBytecodeFile(BytecodeFile &file) {
  if (!file.resolveCodeLabels()) {
    return false;
  }

  //--- bytecode section
  uint32_t bytecodeAddr = (uint32_t)bytecodeSection.size();
  if (bytecodeSection.size() > 0xffffffffU - file.bytecodeSection.size()) {
    (*errorFunc)("Bytecode section too large");
    return false;
  }
  bytecodeSection.insert(bytecodeSection.end(),
			 file.bytecodeSection.begin(), file.bytecodeSection.end());

  //--- data section
  alignData();
  uint32_t dataAddr = (uint32_t)dataSection.size();
  if (dataSection.size() > 0xffffffffU - file.dataSection.size()) {
    (*errorFunc)("Data section too large");
    return false;
  }
  dataSection.insert(dataSection.end(), file.dataSection.begin(), file.dataSection.end());

  //--- function definitions
  for (auto &pair : file.funcDefns) {
    if (funcDefns.find(pair.first) == funcDefns.end()) {
      funcDefns[pair.first] = bytecodeAddr + pair.second;
    } else {
      (*errorFunc)("Function '" + pair.first + "' is defined multiple times");
      return false;
    }
  }

  //--- bytecode relocations
  for (auto &pair : file.bytecodeRelocs) {
    for (uint32_t instrAddr : pair.second) {
      bytecodeRelocs[pair.first].push_back(bytecodeAddr + instrAddr);
    }
  }

  //--- native relocations
  for (auto &pair : file.nativeRelocs) {
    for (uint32_t instrAddr : pair.second) {
      nativeRelocs[pair.first].push_back(bytecodeAddr + instrAddr);
    }
  }

  //--- data labels
  for (DataLabel &dataLabel : file.dataLabels) {
    dataLabels.emplace_back(dataAddr + dataLabel.dataAddr);
    for (uint32_t instrAddr : dataLabel.instrAddrs) {
      dataLabels.back().instrAddrs.push_back(bytecodeAddr + instrAddr);
    }
  }

  return true;
}

//------------------------------------------------------------------------
// bytecode section
//------------------------------------------------------------------------

void BytecodeFile::setFunc(const std::string &name) {
  funcDefns[name] = (uint32_t)bytecodeSection.size();
}

uint32_t BytecodeFile::allocCodeLabel() {
  uint32_t codeLabel = (uint32_t)codeLabels.size();
  codeLabels.emplace_back();
  return codeLabel;
}

void BytecodeFile::setCodeLabel(uint32_t label) {
  codeLabels[label].bytecodeAddr = (uint32_t)bytecodeSection.size();
  codeLabels[label].bytecodeAddrSet = true;
}

bool BytecodeFile::addInstr(uint8_t opcode) {
  if (bytecodeSection.size() > 0xffffffffU - 1) {
    return false;
  }
  bytecodeSection.push_back(opcode);
  return true;
}

bool BytecodeFile::addPushIInstr(int64_t immed) {
  if (bytecodeSection.size() > 0xffffffffU - 8) {
    return false;
  }
  bytecodeSection.push_back(bcOpcodePushI);
  addBytecode((uint8_t *)&immed, 7);
  return true;
}

bool BytecodeFile::addPushFInstr(float immed) {
  if (bytecodeSection.size() > 0xffffffffU - 5) {
    return false;
  }
  bytecodeSection.push_back(bcOpcodePushF);
  addBytecode((uint8_t *)&immed, 4);
  return true;
}

bool BytecodeFile::addPushBcodeInstr(const std::string &funcName) {
  if (bytecodeSection.size() > 0xffffffffU - 8) {
    return false;
  }
  bytecodeSection.push_back(bcOpcodePushBcode);
  bytecodeRelocs[funcName].push_back((uint32_t)bytecodeSection.size());
  bytecodeSection.insert(bytecodeSection.end(), 7, 0);
  return true;
}

bool BytecodeFile::addPushDataInstr(uint32_t dataLabel) {
  if (bytecodeSection.size() > 0xffffffffU - 9) {
    return false;
  }
  bytecodeSection.push_back(bcOpcodePushData);
  dataLabels[dataLabel].instrAddrs.push_back((uint32_t)bytecodeSection.size());
  bytecodeSection.insert(bytecodeSection.end(), 8, 0);
  return true;
}

bool BytecodeFile::addPushNativeInstr(const std::string &funcName) {
  if (bytecodeSection.size() > 0xffffffffU - 9) {
    return false;
  }
  bytecodeSection.push_back(bcOpcodePushNative);
  nativeRelocs[funcName].push_back((uint32_t)bytecodeSection.size());
  bytecodeSection.insert(bytecodeSection.end(), 8, 0);
  return true;
}

bool BytecodeFile::addGetStackInstr(uint32_t idx) {
  if (bytecodeSection.size() > 0xffffffffU - 5) {
    return false;
  }
  bytecodeSection.push_back(bcOpcodeGetStack);
  addBytecode((uint8_t *)&idx, 4);
  return true;
}

bool BytecodeFile::addBranchInstr(uint8_t opcode, uint32_t codeLabel) {
  if (bytecodeSection.size() > 0xffffffffU - 5) {
    return false;
  }
  bytecodeSection.push_back(opcode);
  codeLabels[codeLabel].instrAddrs.push_back((uint32_t)bytecodeSection.size());
  bytecodeSection.insert(bytecodeSection.end(), 4, 0);
  return true;
}

void BytecodeFile::addBytecode(const uint8_t *bytecode, size_t nBytes) {
  bytecodeSection.insert(bytecodeSection.end(), bytecode, bytecode + nBytes);
}

//------------------------------------------------------------------------
// data section
//------------------------------------------------------------------------

uint32_t BytecodeFile::allocAndSetDataLabel() {
  uint32_t dataLabel = (uint32_t)dataLabels.size();
  dataLabels.emplace_back((uint32_t)dataSection.size());
  return dataLabel;
}

bool BytecodeFile::addData(const uint8_t *data, uint32_t nBytes) {
  if (dataSection.size() > 0xffffffff - nBytes) {
    return false;
  }
  dataSection.insert(dataSection.end(), data, data + nBytes);
  return true;
}

void BytecodeFile::alignData() {
  size_t k = dataSection.size() & 7;
  if (k) {
    dataSection.insert(dataSection.end(), 8 - k, 0);
  }
}

//------------------------------------------------------------------------
// linking
//------------------------------------------------------------------------

bool BytecodeFile::resolveRelocs() {
  bool ok = true;

  for (auto &pair : bytecodeRelocs) {
    auto iter = funcDefns.find(pair.first);
    if (iter == funcDefns.end()) {
      errorFunc("Function '" + pair.first + "' is undefined");
      ok = false;
    } else {
      for (uint32_t instrAddr : pair.second) {
	uint64_t addr = iter->second;
	uint8_t *p = (uint8_t *)&addr;
	bytecodeSection[instrAddr    ] = p[0];
	bytecodeSection[instrAddr + 1] = p[1];
	bytecodeSection[instrAddr + 2] = p[2];
	bytecodeSection[instrAddr + 3] = p[3];
	bytecodeSection[instrAddr + 4] = p[4];
	bytecodeSection[instrAddr + 5] = p[5];
	bytecodeSection[instrAddr + 6] = p[6];
      }
    }
  }

  for (DataLabel &dataLabel : dataLabels) {
    for (uint32_t instrAddr : dataLabel.instrAddrs) {
      if (instrAddr + 8 > bytecodeSection.size()) {
	(*errorFunc)("Invalid instruction address for data label");
	ok = false;
      } else {
	uint8_t *p = (uint8_t *)&dataLabel.dataAddr;
	bytecodeSection[instrAddr    ] = p[0];
	bytecodeSection[instrAddr + 1] = p[1];
	bytecodeSection[instrAddr + 2] = p[2];
	bytecodeSection[instrAddr + 3] = p[3];
	bytecodeSection[instrAddr + 4] = 0;
	bytecodeSection[instrAddr + 5] = 0;
	bytecodeSection[instrAddr + 6] = 0;
	bytecodeSection[instrAddr + 7] = 0;
      }
    }
  }

  if (ok) {
    bytecodeRelocs.clear();
    dataLabels.clear();
  }
  return ok;
}

//------------------------------------------------------------------------
// direct access
//------------------------------------------------------------------------

uint32_t BytecodeFile::bytecodeSectionLength() {
  return (uint32_t)bytecodeSection.size();
}

uint8_t BytecodeFile::bytecodeSectionByte(uint32_t bytecodeAddr) {
  return bytecodeSection[bytecodeAddr];
}

void BytecodeFile::takeBytecodeSection(std::vector<uint8_t> &v) {
  std::swap(bytecodeSection, v);
}

void BytecodeFile::forEachFuncDefn(std::function<void(const std::string &funcName,
						      uint32_t bytecodeAddr)> func) {
  for (auto &pair : funcDefns) {
    func(pair.first, pair.second);
  }
}

bool BytecodeFile::hasBytecodeRelocs() {
  return !bytecodeRelocs.empty();
}

void BytecodeFile::forEachBytecodeReloc(
		       std::function<void(const std::string &funcName,
					  const std::vector<uint32_t> &instrAddrs)> func) {
  for (auto &pair : bytecodeRelocs) {
    func(pair.first, pair.second);
  }
}

void BytecodeFile::forEachNativeReloc(
		       std::function<void(const std::string &funcName,
					  const std::vector<uint32_t> &instrAddrs)> func) {
  for (auto &pair : nativeRelocs) {
    func(pair.first, pair.second);
  }
}

uint32_t BytecodeFile::dataSectionLength() {
  return (uint32_t)dataSection.size();
}

uint8_t BytecodeFile::dataSectionByte(uint32_t dataAddr) {
  return dataSection[dataAddr];
}

void BytecodeFile::takeDataSection(std::vector<uint8_t> &v) {
  std::swap(dataSection, v);
}

bool BytecodeFile::hasDataLabels() {
  return !dataLabels.empty();
}

void BytecodeFile::forEachDataLabel(
		       std::function<void(uint32_t idx, uint32_t dataAddr,
					  const std::vector<uint32_t> &instrAddrs)>func) {
  for (size_t i = 0; i < dataLabels.size(); ++i) {
    func(i, dataLabels[i].dataAddr, dataLabels[i].instrAddrs);
  }
}
