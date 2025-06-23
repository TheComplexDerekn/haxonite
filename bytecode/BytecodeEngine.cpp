//========================================================================
//
// BytecodeEngine.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "BytecodeEngine.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "BytecodeDefs.h"
#include "BytecodeFile.h"
#include "SysIO.h"

//------------------------------------------------------------------------
// load and run
//------------------------------------------------------------------------

BytecodeEngine::BytecodeEngine(const std::string &configPath,
			       size_t aStackSize, size_t aInitialHeapSize,
			       bool aVerbose) {
  loadConfigFile(configPath);
  stackSize = aStackSize;
  initialHeapSize = aInitialHeapSize;
  verbose = aVerbose;
  try {
    stack = std::unique_ptr<Cell[]>(new Cell[stackSize]);
  } catch (std::bad_alloc) {
    fatalError("Out of memory");
  }
  for (size_t i = 0; i < stackSize; ++i) {
    stack[i] = cellMakeInt(0);
  }
  heapInit();
  sp = stackSize;
  fp = stackSize;
  ap = stackSize;
  pc = 0;
}

void BytecodeEngine::loadConfigFile(const std::string &configPath) {
  std::string configPath2;
  if (configPath.empty()) {
    configPath2 = configDir() + "/" + ".haxoniterc";
  } else {
    configPath2 = configPath;
  }
  if (!cfg.load(configPath2, [configPath2](int lineNum, const std::string &msg) {
	fprintf(stderr, "Error in config file [%s:%d]: %s\n",
		configPath2.c_str(), lineNum, msg.c_str());
      })) {
    fatalError("Invalid config file");
  }
}

bool BytecodeEngine::loadBytecodeFile(const std::string &path) {
  return load(path);
}

bool BytecodeEngine::callFunction(const std::string &name, int nArgs) {
  auto iter = funcDefns.find(name);
  if (iter == funcDefns.end()) {
    return false;
  }
  if (iter->second >= bytecode.size()) {
    fatalError("Invalid function bytecode address");
  }
  if (sp + nArgs > stackSize) {
    fatalError("Stack underflow");
  }

  pc = iter->second;
  push(cellMakeSavedReg(0));
  push(cellMakeSavedReg(ap));
  push(cellMakeSavedReg(fp));
  fp = sp;
  ap = fp + 2 + nArgs;
  pc = iter->second;
  run();

  return true;
}

void BytecodeEngine::callFunctionPtr(Cell &funcPtrCell, int nArgs) {
  if (sp + nArgs > stackSize) {
    fatalError("Stack underflow");
  }
  if (!cellIsHeapPtr(funcPtrCell)) {
    fatalError("Invalid function pointer");
  }
  Cell *funcPtr = (Cell *)cellHeapPtr(funcPtrCell);
  if (heapObjGCTag(funcPtr) != gcTagTuple) {
    fatalError("Invalid function pointer");
  }
  int64_t funcPtrSize = heapObjSize(funcPtr) / 8;
  if (funcPtrSize < 1) {
    fatalError("Invalid function pointer");
  }
  int64_t nInitialArgs = funcPtrSize - 1;
  if (nInitialArgs > 0) {
    // before          after
    // ------------    --------------------------
    //                 arg[nArgs-1]
    //                 ...
    //                 arg[3]
    // arg[nArgs-1]    arg[2]
    // ...             arg[1]
    // arg[3]          arg[0]
    // arg[2]          initialArg[nInitialArgs-1]
    // arg[1]          ...
    // arg[0]          initialArg[0]
    if (sp < nInitialArgs) {
      fatalError("Stack overflow");
    }
    sp -= nInitialArgs;
    for (int64_t i = 0; i < nArgs; ++i) {
      stack[sp + i] = stack[sp + i + nInitialArgs];
    }
    for (int64_t i = 0; i < nInitialArgs; ++i) {
      stack[sp + nArgs + nInitialArgs - 1 - i] = funcPtr[2 + i];
    }
  }
  push(cellMakeSavedReg(0));
  push(cellMakeSavedReg(ap));
  push(cellMakeSavedReg(fp));
  fp = sp;
  ap = fp + 2 + nArgs + nInitialArgs;
  Cell func = funcPtr[1];
  if (cellIsBytecodeAddr(func)) {
    pc = cellBytecodeAddr(func);
  } else if (cellIsNativePtr(func)) {
    pc = 0;
    (*cellNativePtr(func))(*this);
    doReturn();
    if (pc == 0) {
      fatalError("Invalid bytecode return address");
    }
  } else {
    fatalError("Invalid operand");
  }
  run();
}

//------------------------------------------------------------------------
// setup
//------------------------------------------------------------------------

void BytecodeEngine::addNativeFunction(const std::string &name, NativeFunc func) {
  nativeFuncs[name] = func;
}

//------------------------------------------------------------------------
// support for native functions
//------------------------------------------------------------------------

int BytecodeEngine::nArgs() {
  return ap - (fp + 2);
}

Cell &BytecodeEngine::arg(int idx) {
  if (idx < 0 || idx >= nArgs()) {
    fatalError("Out of call frame bounds");
  }
  return stack[ap - idx];
}

void BytecodeEngine::push(Cell cell) {
  if (sp == 0) {
    fatalError("Stack overflow");
  }
  stack[--sp] = cell;
}

Cell BytecodeEngine::pop() {
  if (sp >= stackSize) {
    fatalError("Stack underflow");
  }
  return stack[sp++];
}

int64_t BytecodeEngine::popInt() {
  Cell cell = pop();
  if (!cellIsInt(cell)) {
    fatalError("Cell type mismatch");
  }
  return cellInt(cell);
}

float BytecodeEngine::popFloat() {
  Cell cell = pop();
  if (!cellIsFloat(cell)) {
    fatalError("Cell type mismatch");
  }
  return cellFloat(cell);
}

bool BytecodeEngine::popBool() {
  Cell cell = pop();
  if (!cellIsBool(cell)) {
    fatalError("Cell type mismatch");
  }
  return cellBool(cell);
}

size_t BytecodeEngine::popBytecodeAddr() {
  Cell cell = pop();
  if (!cellIsBytecodeAddr(cell)) {
    fatalError("Cell type mismatch");
  }
  return cellBytecodeAddr(cell);
}

size_t BytecodeEngine::popSavedReg() {
  Cell cell = pop();
  if (!cellIsSavedReg(cell)) {
    fatalError("Cell type mismatch");
  }
  return cellSavedReg(cell);
}

void *BytecodeEngine::popHeapPtr() {
  Cell cell = pop();
  if (!cellIsHeapPtr(cell)) {
    fatalError("Cell type mismatch");
  }
  return cellHeapPtr(cell);
}

void *BytecodeEngine::popNonHeapPtr() {
  Cell cell = pop();
  if (!cellIsNonHeapPtr(cell)) {
    fatalError("Cell type mismatch");
  }
  return cellNonHeapPtr(cell);
}

void *BytecodeEngine::popPtr() {
  Cell cell = pop();
  if (!cellIsPtr(cell)) {
    fatalError("Cell type mismatch");
  }
  return cellPtr(cell);
}

NativeFunc BytecodeEngine::popNativeFunc() {
  Cell cell = pop();
  if (!cellIsNativePtr(cell)) {
    fatalError("Cell type mismatch");
  }
  return cellNativePtr(cell);
}

//------------------------------------------------------------------------
// loader
//------------------------------------------------------------------------

static void bcError(const std::string &msg) {
  fprintf(stderr, "BYTECODE ERROR: %s\n", msg.c_str());
}

bool BytecodeEngine::load(const std::string &path) {
  BytecodeFile bcFile(bcError);
  if (!bcFile.read(path)) {
    return false;
  }
  bcFile.takeBytecodeSection(bytecode);
  bcFile.takeDataSection(data);
  bool ok = true;
  bcFile.forEachFuncDefn([&](const std::string &funcName, uint32_t bytecodeAddr) {
      funcDefns[funcName] = bytecodeAddr;
    });
  if (bcFile.hasBytecodeRelocs()) {
    bcError("Not an executable bytecode file - has bytecode relocs");
    ok = false;
  }
  bcFile.forEachNativeReloc([&](const std::string &funcName,
				const std::vector<uint32_t> &instrAddrs) {
      auto iter = nativeFuncs.find(funcName);
      if (iter == nativeFuncs.end()) {
	bcError("Undefined native function '" + funcName + "'");
	ok = false;
      }
      uint64_t funcPtr = (uint64_t)iter->second;
      for (uint32_t instrAddr : instrAddrs) {
	ok &= writeBytecodeUint64(instrAddr, funcPtr);
      }
    });
  if (bcFile.hasDataLabels()) {
    bcError("Not an executable bytecode file - has data labels");
    ok = false;
  }
  return ok;
}

//------------------------------------------------------------------------
// interpreter
//------------------------------------------------------------------------

void BytecodeEngine::run() {
  while (true) {

#if 0 //~debug
    printf("pc=%zx sp=%zx fp=%zx ap=%zx | stack:", pc, sp, fp, ap);
    for (size_t i = stackSize - 1; i >= sp; --i) {
      printf(" %016lx", stack[i]);
    }
    printf("\n");
#endif

    uint8_t opcode = readBytecodeUint8();
    switch (opcode) {
    case bcOpcodePushI:
      push(cellMakeInt(readBytecodeInt56()));
      break;
    case bcOpcodePushF:
      push(cellMakeFloat(readBytecodeFloat32()));
      break;
    case bcOpcodePushTrue:
      push(cellMakeBool(true));
      break;
    case bcOpcodePushFalse:
      push(cellMakeBool(false));
      break;
    case bcOpcodePushBcode:
      push(cellMakeBytecodeAddr((size_t)readBytecodeUint56()));
      break;
    case bcOpcodePushData:
      push(cellMakeNonHeapPtr(&data[readBytecodeUint64()]));
      break;
    case bcOpcodePushNative:
      push(cellMakeNativePtr((NativeFunc)readBytecodeUint64()));
      break;
    case bcOpcodePushNil:
      push(cellMakeNilHeapPtr());
      break;
    case bcOpcodePushError:
      push(cellMakeError());
      break;
    case bcOpcodePop:
      pop();
      break;
    case bcOpcodeGetArg: {
      int64_t argIdx = popInt();
      if (argIdx < 0 || argIdx >= ap - (fp + 2)) {
	fatalError("Out of call frame bounds");
      }
      push(stack[ap - argIdx]);
      break;
    }
    case bcOpcodeGetVar: {
      int64_t varIdx = popInt();
      if (varIdx < 1 || varIdx > fp - sp) {
	fatalError("Out of call frame bounds");
      }
      push(stack[fp - varIdx]);
      break;
    }
    case bcOpcodePutVar: {
      int64_t varIdx = popInt();
      if (varIdx < 1 || varIdx > fp - sp) {
	fatalError("Out of call frame bounds");
      }
      stack[fp - varIdx] = pop();
      break;
    }
    case bcOpcodeTestValid:
      push(cellMakeBool(!cellIsError(pop())));
      break;
    case bcOpcodeCheckValid: {
      if (sp >= stackSize) {
	fatalError("Stack underflow");
      }
      if (cellIsError(stack[sp])) {
	fatalError("Uncaught error");
      }
      break;
    }
    case bcOpcodeGetStack: {
      uint32_t idx = readBytecodeUint32();
      if (idx >= stackSize - sp) {
	fatalError("Stack underflow");
      }
      push(stack[sp + idx]);
      break;
    }
    case bcOpcodeCall: {
      Cell func = pop();
      int64_t nArgs = popInt();
      if (sp + nArgs > stackSize) {
	fatalError("Out of call frame bounds");
      }
      push(cellMakeSavedReg(pc));
      push(cellMakeSavedReg(ap));
      push(cellMakeSavedReg(fp));
      fp = sp;
      ap = sp + 2 + nArgs;
      if (cellIsBytecodeAddr(func)) {
	pc = cellBytecodeAddr(func);
      } else if (cellIsNativePtr(func)) {
	pc = 0;
	(*cellNativePtr(func))(*this);
	doReturn();
	if (pc == 0) {
	  fatalError("Invalid bytecode return address");
	}
      } else {
	fatalError("Invalid operand");
      }
      break;
    }
    case bcOpcodePtrcall: {
      Cell *funcPtr = (Cell *)popHeapPtr();
      int64_t nArgs = popInt();
      if (sp + nArgs > stackSize) {
	fatalError("Out of call frame bounds");
      }
      if (heapObjGCTag(funcPtr) != gcTagTuple) {
	fatalError("Invalid function pointer");
      }
      int64_t funcPtrSize = heapObjSize(funcPtr) / 8;
      if (funcPtrSize < 1) {
	fatalError("Invalid function pointer");
      }
      int64_t nInitialArgs = funcPtrSize - 1;
      if (nInitialArgs > 0) {
	// before          after
	// ------------    --------------------------
	//                 arg[nArgs-1]
	//                 ...
	//                 arg[3]
	// arg[nArgs-1]    arg[2]
	// ...             arg[1]
	// arg[3]          arg[0]
	// arg[2]          initialArg[nInitialArgs-1]
	// arg[1]          ...
	// arg[0]          initialArg[0]
	if (sp < nInitialArgs) {
	  fatalError("Stack overflow");
	}
	sp -= nInitialArgs;
	for (int64_t i = 0; i < nArgs; ++i) {
	  stack[sp + i] = stack[sp + i + nInitialArgs];
	}
	for (int64_t i = 0; i < nInitialArgs; ++i) {
	  stack[sp + nArgs + nInitialArgs - 1 - i] = funcPtr[2 + i];
	}
      }
      push(cellMakeSavedReg(pc));
      push(cellMakeSavedReg(ap));
      push(cellMakeSavedReg(fp));
      fp = sp;
      ap = sp + 2 + nArgs + nInitialArgs;
      Cell func = funcPtr[1];
      if (cellIsBytecodeAddr(func)) {
	pc = cellBytecodeAddr(func);
      } else if (cellIsNativePtr(func)) {
	pc = 0;
	(*cellNativePtr(func))(*this);
	doReturn();
	if (pc == 0) {
	  fatalError("Invalid bytecode return address");
	}
      } else {
	fatalError("Invalid operand");
      }
      break;
    }
    case bcOpcodeReturn: {
      doReturn();
      if (pc == 0) {
	// return to native code
	return;
      }
      break;
    }
    case bcOpcodeBranchTrue: {
      int32_t relOffset = readBytecodeInt32();
      bool flag = popBool();
      if (flag) {
	if ((relOffset > 0 && relOffset >= bytecode.size() - pc) ||
	    (relOffset < 0 && -relOffset > pc)) {
	  fatalError("Invalid branch destination");
	}
	pc += relOffset;
      }
      break;
    }
    case bcOpcodeBranchFalse: {
      int32_t relOffset = readBytecodeInt32();
      bool flag = popBool();
      if (!flag) {
	if ((relOffset > 0 && relOffset >= bytecode.size() - pc) ||
	    (relOffset < 0 && -relOffset > pc)) {
	  fatalError("Invalid branch destination");
	}
	pc += relOffset;
      }
      break;
    }
    case bcOpcodeBranch: {
      int32_t relOffset = readBytecodeInt32();
      if ((relOffset > 0 && relOffset >= bytecode.size() - pc) ||
	  (relOffset < 0 && -relOffset > pc)) {
        fatalError("Invalid branch destination");
      }
      pc += relOffset;
      break;
    }
    case bcOpcodeLoad: {
      int64_t idx = popInt();
      void *ptr = popPtr();
      if (!ptr) {
	fatalError("Nil pointer dereference");
      }
      if (heapObjGCTag(ptr) != gcTagTuple ||
	  idx < 0 ||
	  idx >= heapObjSize(ptr) / 8) {
	fatalError("Invalid load address");
      }
      push(((Cell *)ptr)[1 + idx]);
      break;
    }
    case bcOpcodeStore: {
      int64_t idx = popInt();
      void *ptr = popPtr();
      Cell value = pop();
      if (!ptr) {
	fatalError("Nil pointer dereference");
      }
      if (heapObjGCTag(ptr) != gcTagTuple ||
	  idx < 0 ||
	  idx >= heapObjSize(ptr) / 8) {
	fatalError("Invalid store address");
      }
      ((Cell *)ptr)[1 + idx] = value;
      break;
    }
    case bcOpcodeAdd: {
      Cell op2 = pop();
      Cell op1 = pop();
      if (cellIsInt(op1) && cellIsInt(op2)) {
	int64_t result = cellInt(op1) + cellInt(op2);
	if (result > bytecodeMaxInt || result < bytecodeMinInt) {
	  fatalError("Integer overflow");
	}
	push(cellMakeInt(result));
      } else if (cellIsFloat(op1) && cellIsFloat(op2)) {
	float result = cellFloat(op1) + cellFloat(op2);
	push(cellMakeFloat(result));
      } else {
	fatalError("Invalid operand");
      }
      break;
    }
    case bcOpcodeSub: {
      Cell op2 = pop();
      Cell op1 = pop();
      if (cellIsInt(op1) && cellIsInt(op2)) {
	int64_t result = cellInt(op1) - cellInt(op2);
	if (result > bytecodeMaxInt || result < bytecodeMinInt) {
	  fatalError("Integer overflow");
	}
	push(cellMakeInt(result));
      } else if (cellIsFloat(op1) && cellIsFloat(op2)) {
	float result = cellFloat(op1) - cellFloat(op2);
	push(cellMakeFloat(result));
      } else {
	fatalError("Invalid operand");
      }
      break;
    }
    case bcOpcodeMul: {
      Cell op2 = pop();
      Cell op1 = pop();
      if (cellIsInt(op1) && cellIsInt(op2)) {
	int64_t result;
	if (__builtin_mul_overflow(cellInt(op1), cellInt(op2), &result) ||
	    result > bytecodeMaxInt || result < bytecodeMinInt) {
	  fatalError("Integer overflow");
	}
	push(cellMakeInt(result));
      } else if (cellIsFloat(op1) && cellIsFloat(op2)) {
	float result = cellFloat(op1) * cellFloat(op2);
	push(cellMakeFloat(result));
      } else {
	fatalError("Invalid operand");
      }
      break;
    }
    case bcOpcodeDiv: {
      Cell op2 = pop();
      Cell op1 = pop();
      if (cellIsInt(op1) && cellIsInt(op2)) {
	int64_t i2 = cellInt(op2);
	if (i2 == 0) {
	  fatalError("Integer divide-by-zero");
	}
	int64_t result = cellInt(op1) / i2;
	// overflow case: minInt / -1 --> maxInt + 1
	if (result > bytecodeMaxInt) {
	  fatalError("Integer overflow");
	}
	push(cellMakeInt(result));
      } else if (cellIsFloat(op1) && cellIsFloat(op2)) {
	float result = cellFloat(op1) / cellFloat(op2);
	push(cellMakeFloat(result));
      } else {
	fatalError("Invalid operand");
      }
      break;
    }
    case bcOpcodeMod: {
      Cell op2 = pop();
      Cell op1 = pop();
      if (cellIsInt(op1) && cellIsInt(op2)) {
	int64_t i2 = cellInt(op2);
	if (i2 == 0) {
	  fatalError("Integer divide-by-zero");
	}
	int64_t result = cellInt(op1) % i2;
	push(cellMakeInt(result));
      } else {
	fatalError("Invalid operand");
      }
      break;
    }
    case bcOpcodeOr: {
      Cell op2 = pop();
      Cell op1 = pop();
      if (cellIsInt(op1) && cellIsInt(op2)) {
	int64_t result = cellInt(op1) | cellInt(op2);
	push(cellMakeInt(result));
      } else if (cellIsBool(op1) && cellIsBool(op2)) {
	bool result = cellBool(op1) | cellBool(op2);
	push(cellMakeBool(result));
      } else {
	fatalError("Invalid operand");
      }
      break;
    }
    case bcOpcodeXor: {
      Cell op2 = pop();
      Cell op1 = pop();
      if (cellIsInt(op1) && cellIsInt(op2)) {
	int64_t result = cellInt(op1) ^ cellInt(op2);
	push(cellMakeInt(result));
      } else if (cellIsBool(op1) && cellIsBool(op2)) {
	bool result = cellBool(op1) ^ cellBool(op2);
	push(cellMakeBool(result));
      } else {
	fatalError("Invalid operand");
      }
      break;
    }
    case bcOpcodeAnd: {
      Cell op2 = pop();
      Cell op1 = pop();
      if (cellIsInt(op1) && cellIsInt(op2)) {
	int64_t result = cellInt(op1) & cellInt(op2);
	push(cellMakeInt(result));
      } else if (cellIsBool(op1) && cellIsBool(op2)) {
	bool result = cellBool(op1) & cellBool(op2);
	push(cellMakeBool(result));
      } else {
	fatalError("Invalid operand");
      }
      break;
    }
    case bcOpcodeSll: {
      Cell op2 = pop();
      Cell op1 = pop();
      if (cellIsInt(op1) && cellIsInt(op2)) {
	int64_t result = cellInt(op1) << cellInt(op2);
	push(cellMakeInt(result));
      } else {
	fatalError("Invalid operand");
      }
      break;
    }
    case bcOpcodeSrl: {
      Cell op2 = pop();
      Cell op1 = pop();
      if (cellIsInt(op1) && cellIsInt(op2)) {
	int64_t result = (int64_t)((uint64_t)cellInt(op1) >> cellInt(op2));
	push(cellMakeInt(result));
      } else {
	fatalError("Invalid operand");
      }
      break;
    }
    case bcOpcodeSra: {
      Cell op2 = pop();
      Cell op1 = pop();
      if (cellIsInt(op1) && cellIsInt(op2)) {
	int64_t result = cellInt(op1) >> cellInt(op2);
	push(cellMakeInt(result));
      } else {
	fatalError("Invalid operand");
      }
      break;
    }
    case bcOpcodeNeg: {
      Cell op = pop();
      if (cellIsInt(op)) {
	int64_t result = -cellInt(op);
	// overflow case: -minInt --> maxInt + 1
	if (result > bytecodeMaxInt) {
	  fatalError("Integer overflow");
	}
	push(cellMakeInt(result));
      } else if (cellIsFloat(op)) {
	float result = -cellFloat(op);
	push(cellMakeFloat(result));
      } else {
	fatalError("Invalid operand");
      }
      break;
    }
    case bcOpcodeNot: {
      Cell op = pop();
      if (cellIsInt(op)) {
	int64_t result = ~cellInt(op);
	push(cellMakeInt(result));
      } else if (cellIsBool(op)) {
	bool result = !cellBool(op);
	push(cellMakeBool(result));
      } else {
	fatalError("Invalid operand");
      }
      break;
    }
    case bcOpcodeCmpeq: {
      Cell op2 = pop();
      Cell op1 = pop();
      if (!(cellType(op1) == cellType(op2)) &&
	  !(cellIsPtr(op1) && cellIsPtr(op2))) {
	fatalError("Invalid operand");
      }
      push(cellMakeBool(op1 == op2));
      break;
    }
    case bcOpcodeCmpne: {
      Cell op2 = pop();
      Cell op1 = pop();
      if (!(cellType(op1) == cellType(op2)) &&
	  !(cellIsPtr(op1) && cellIsPtr(op2))) {
	fatalError("Invalid operand");
      }
      push(cellMakeBool(op1 != op2));
      break;
    }
    case bcOpcodeCmplt: {
      Cell op2 = pop();
      Cell op1 = pop();
      bool result;
      if (cellIsInt(op1) && cellIsInt(op2)) {
	result = cellInt(op1) < cellInt(op2);
      } else if (cellIsFloat(op1) && cellIsFloat(op2)) {
	result = cellFloat(op1) < cellFloat(op2);
      } else {
	fatalError("Invalid operand");
      }
      push(cellMakeBool(result));
      break;
    }
    case bcOpcodeCmpgt: {
      Cell op2 = pop();
      Cell op1 = pop();
      bool result;
      if (cellIsInt(op1) && cellIsInt(op2)) {
	result = cellInt(op1) > cellInt(op2);
      } else if (cellIsFloat(op1) && cellIsFloat(op2)) {
	result = cellFloat(op1) > cellFloat(op2);
      } else {
	fatalError("Invalid operand");
      }
      push(cellMakeBool(result));
      break;
    }
    case bcOpcodeCmple: {
      Cell op2 = pop();
      Cell op1 = pop();
      bool result;
      if (cellIsInt(op1) && cellIsInt(op2)) {
	result = cellInt(op1) <= cellInt(op2);
      } else if (cellIsFloat(op1) && cellIsFloat(op2)) {
	result = cellFloat(op1) <= cellFloat(op2);
      } else {
	fatalError("Invalid operand");
      }
      push(cellMakeBool(result));
      break;
    }
    case bcOpcodeCmpge: {
      Cell op2 = pop();
      Cell op1 = pop();
      bool result;
      if (cellIsInt(op1) && cellIsInt(op2)) {
	result = cellInt(op1) >= cellInt(op2);
      } else if (cellIsFloat(op1) && cellIsFloat(op2)) {
	result = cellFloat(op1) >= cellFloat(op2);
      } else {
	fatalError("Invalid operand");
      }
      push(cellMakeBool(result));
      break;
    }
    default:
      fatalError("Invalid instruction");
    }
  }
}

void BytecodeEngine::doReturn() {
  Cell returnValue = pop();
  size_t newSP = ap + 1;
  if (newSP > stackSize) {
    fatalError("Stack underflow");
  }
  sp = fp;
  fp = popSavedReg();
  ap = popSavedReg();
  pc = popSavedReg();
  sp = newSP;
  push(returnValue);
}

//------------------------------------------------------------------------
// bytecode data access
//------------------------------------------------------------------------

uint8_t BytecodeEngine::readBytecodeUint8() {
  if (pc > bytecode.size()) {
    fatalError("Invalid bytecode address");
  }
  return bytecode[pc++];
}

int32_t BytecodeEngine::readBytecodeInt32() {
  if (pc + 4 > bytecode.size()) {
    fatalError("Invalid bytecode address");
  }
  int32_t val =  (int32_t)bytecode[pc    ]        |
                ((int32_t)bytecode[pc + 1] <<  8) |
                ((int32_t)bytecode[pc + 2] << 16) |
                ((int32_t)bytecode[pc + 3] << 24);
  pc += 4;
  return val;
}

uint32_t BytecodeEngine::readBytecodeUint32() {
  if (pc + 4 > bytecode.size()) {
    fatalError("Invalid bytecode address");
  }
  uint32_t val =  (uint32_t)bytecode[pc    ]        |
                 ((uint32_t)bytecode[pc + 1] <<  8) |
                 ((uint32_t)bytecode[pc + 2] << 16) |
                 ((uint32_t)bytecode[pc + 3] << 24);
  pc += 4;
  return val;
}

int64_t BytecodeEngine::readBytecodeInt56() {
  if (pc + 7 > bytecode.size()) {
    fatalError("Invalid bytecode address");
  }
  int64_t val =  (int64_t)bytecode[pc    ]        |
                ((int64_t)bytecode[pc + 1] <<  8) |
                ((int64_t)bytecode[pc + 2] << 16) |
                ((int64_t)bytecode[pc + 3] << 24) |
                ((int64_t)bytecode[pc + 4] << 32) |
                ((int64_t)bytecode[pc + 5] << 40) |
                ((int64_t)bytecode[pc + 6] << 48);
  if (val & ((int64_t)1 << 55)) {
    val |= (int64_t)0xff << 56;
  }
  pc += 7;
  return val;
}

uint64_t BytecodeEngine::readBytecodeUint56() {
  if (pc + 7 > bytecode.size()) {
    fatalError("Invalid bytecode address");
  }
  uint64_t val =  (uint64_t)bytecode[pc    ]        |
                 ((uint64_t)bytecode[pc + 1] <<  8) |
                 ((uint64_t)bytecode[pc + 2] << 16) |
                 ((uint64_t)bytecode[pc + 3] << 24) |
                 ((uint64_t)bytecode[pc + 4] << 32) |
                 ((uint64_t)bytecode[pc + 5] << 40) |
                 ((uint64_t)bytecode[pc + 6] << 48);
  pc += 7;
  return val;
}

uint64_t BytecodeEngine::readBytecodeUint64() {
  if (pc + 8 > bytecode.size()) {
    fatalError("Invalid bytecode address");
  }
  uint64_t val =  (uint64_t)bytecode[pc    ]        |
                 ((uint64_t)bytecode[pc + 1] <<  8) |
                 ((uint64_t)bytecode[pc + 2] << 16) |
                 ((uint64_t)bytecode[pc + 3] << 24) |
                 ((uint64_t)bytecode[pc + 4] << 32) |
                 ((uint64_t)bytecode[pc + 5] << 40) |
                 ((uint64_t)bytecode[pc + 6] << 48) |
                 ((uint64_t)bytecode[pc + 7] << 56);
  pc += 8;
  return val;
}

float BytecodeEngine::readBytecodeFloat32() {
  if (pc + 4 > bytecode.size()) {
    fatalError("Invalid bytecode address");
  }
  union {
    uint32_t i;
    float f;
  } u;
  u.i =  (uint32_t)bytecode[pc    ]        |
        ((uint32_t)bytecode[pc + 1] <<  8) |
        ((uint32_t)bytecode[pc + 2] << 16) |
        ((uint32_t)bytecode[pc + 3] << 24);
  pc += 4;
  return u.f;
}

bool BytecodeEngine::writeBytecodeUint64(size_t addr, uint64_t value) {
  if (addr + 8 > bytecode.size()) {
    return false;
  }
  bytecode[addr]     = (uint8_t) value;
  bytecode[addr + 1] = (uint8_t)(value >>  8);
  bytecode[addr + 2] = (uint8_t)(value >> 16);
  bytecode[addr + 3] = (uint8_t)(value >> 24);
  bytecode[addr + 4] = (uint8_t)(value >> 32);
  bytecode[addr + 5] = (uint8_t)(value >> 40);
  bytecode[addr + 6] = (uint8_t)(value >> 48);
  bytecode[addr + 7] = (uint8_t)(value >> 56);
  return true;
}

//------------------------------------------------------------------------
// config file
//------------------------------------------------------------------------

ConfigFile::Item *BytecodeEngine::configItem(const std::string &sectionTag,
					     const std::string &cmd) {
  return cfg.item(sectionTag, cmd);
}

//------------------------------------------------------------------------
// fatal errors
//------------------------------------------------------------------------

[[noreturn]] void BytecodeEngine::fatalError(const char *msg) {
  fprintf(stderr, "FATAL ERROR: %s\n", msg);
  exit(1);
}
