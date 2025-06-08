//========================================================================
//
// BytecodeFile.h
//
// Bytecode file reading and writing.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef BytecodeFile_h
#define BytecodeFile_h

#include <stdint.h>
#include <stdio.h>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

//------------------------------------------------------------------------

using BytecodeFileErrorFunc = void (*)(const std::string &msg);

//------------------------------------------------------------------------

class BytecodeFile {
public:

  BytecodeFile(BytecodeFileErrorFunc aErrorFunc): errorFunc(aErrorFunc) {}

  // Clear any existing content and read the bytecode file from
  // [path]. Returns true on success.
  bool read(const std::string &path);

  // Write the bytecode file to [path]. Returns true on success.
  bool write(const std::string &path);

  // Reset this BytecodeFile to empty.
  void clear();

  // Append another ByteCode file to this one. Updates all of the
  // relocations. Returns true on success.
  bool appendBytecodeFile(BytecodeFile &file);

  //--- instructions

  // Add a function definition (public symbol) for [name], to the
  // current bytecode address.
  void setFunc(const std::string &name);

  // Allocate a local bytecode label (private symbol). This label can
  // be passed to setCodeLabel() and addBranchInstr(), but only within
  // this BytecodeFile.
  uint32_t allocCodeLabel();

  // Set the location for [label] to the current bytecode address. The
  // label must have been generated with allocCodeLabel().
  void setCodeLabel(uint32_t label);

  // Add a bytecode instruction with no immediate operand.
  bool addInstr(uint8_t opcode);

  // Add a push.i instruction.
  bool addPushIInstr(int64_t immed);

  // Add a push.f instruction.
  bool addPushFInstr(float immed);

  // Add a push.bcode instruction. This adds a bytecode relocation
  // entry for [funcName].
  bool addPushBcodeInstr(const std::string &funcName);

  // Add a push.data instruction. This adds a data relocation entry
  // for [dataLabel], which must have been generated with
  // allocAndSetDataLabel().
  bool addPushDataInstr(uint32_t dataLabel);

  // Add a push.native instruction. This adds a native function
  // relocation entry for [funcName].
  bool addPushNativeInstr(const std::string &funcName);

  // Add a get.stack instruction.
  bool addGetStackInstr(uint32_t idx);

  // Add a branch instruction, with target [codeLabel], which must
  // have been generated with allocCodeLabel(). Note that
  // setCodeLabel() can be called before addBranchInstr() (for a
  // backward branch) or after addBranchInstr() (for a forward
  // branch).
  bool addBranchInstr(uint8_t opcode, uint32_t codeLabel);

  //--- data

  // Allocate a local data label (private symbol), and set it to the
  // current data address. This label can be passed to
  // addPushDataInstr(), but only within this BytecodeFile.
  uint32_t allocAndSetDataLabel();

  // Add [nBytes] from [data] to the data section.
  bool addData(const uint8_t *data, uint32_t nBytes);

  // Append 0-7 zero bytes to the data section to align the next
  // object to an 8-byte bonudary.
  void alignData();

  //--- linking

  // Resolve all bytecode relocations and data labels, making this a
  // valid bytecode executable file. Returns true on success.
  bool resolveRelocs();

  //--- direct access

  uint32_t bytecodeSectionLength();
  uint8_t bytecodeSectionByte(uint32_t bytecodeAddr);
  void takeBytecodeSection(std::vector<uint8_t> &v);
  void forEachFuncDefn(std::function<void(const std::string &funcName,
					  uint32_t bytecodeAddr)> func);
  bool hasBytecodeRelocs();
  void forEachBytecodeReloc(std::function<void(const std::string &funcName,
					       const std::vector<uint32_t> &instrAddrs)> func);
  void forEachNativeReloc(std::function<void(const std::string &funcName,
					     const std::vector<uint32_t> &instrAddrs)> func);
  uint32_t dataSectionLength();
  uint8_t dataSectionByte(uint32_t dataAddr);
  void takeDataSection(std::vector<uint8_t> &v);
  bool hasDataLabels();
  void forEachDataLabel(std::function<void(uint32_t idx, uint32_t dataAddr,
					   const std::vector<uint32_t> &instrAddrs)> func);

private:

  struct CodeLabel {
    CodeLabel(): bytecodeAddrSet(false) {}
    bool bytecodeAddrSet;
    uint32_t bytecodeAddr;		// address to which the label refers
    std::vector<uint32_t> instrAddrs;	// instructions that reference this label
  };

  struct DataLabel {
    DataLabel(uint32_t aDataAddr): dataAddr(aDataAddr) {}
    uint32_t dataAddr;			// address to which the label refers
    std::vector<uint32_t> instrAddrs;	// instructions that reference this label
  };

  bool readHeader(FILE *in);
  bool readBytecodeSection(FILE *in);
  bool readDataSection(FILE *in);
  bool readFuncDefns(FILE *in);
  bool readBytecodeRelocs(FILE *in);
  bool readNativeRelocs(FILE *in);
  bool readDataLabels(FILE *in);
  bool readName(std::string &name, FILE *in);
  bool readBytes(void *buf, size_t nBytes, FILE *in);
  void writeHeader(FILE *out);
  void writeBytecodeSection(FILE *out);
  void writeDataSection(FILE *out);
  void writeFuncDefns(FILE *out);
  void writeBytecodeRelocs(FILE *out);
  void writeNativeRelocs(FILE *out);
  void writeDataLabels(FILE *out);
  void writeName(const std::string &name, FILE *out);
  bool resolveCodeLabels();
  void addBytecode(const uint8_t *bytecode, size_t nBytes);

  BytecodeFileErrorFunc errorFunc;

  std::vector<uint8_t> bytecodeSection;
  std::vector<uint8_t> dataSection;

  // function definitions, i.e., public bytecode symbols
  std::unordered_map<std::string, uint32_t> funcDefns;

  // map from a bytecode function name to a list of instruction addresses
  std::unordered_map<std::string, std::vector<uint32_t>> bytecodeRelocs;

  // map from a native function name to a list of instruction addresses
  std::unordered_map<std::string, std::vector<uint32_t>> nativeRelocs;

  // code labels are resolved internally, and not written to the bytecode file
  std::vector<CodeLabel> codeLabels;

  // data labels are written to the bytecode file
  std::vector<DataLabel> dataLabels;
};

#endif // BytecodeFile_h
