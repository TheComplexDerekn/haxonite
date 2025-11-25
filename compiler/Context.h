//========================================================================
//
// Context.h
//
// Code generation context.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef Context_h
#define Context_h

#include <string>
#include <unordered_map>
#include <vector>
#include "AST.h"
#include "CTree.h"

//------------------------------------------------------------------------

class Frame {
public:

  Frame(int aFrameSize)
    : frameSize(aFrameSize)
    , hasLoop(false), continueLabel(0), breakLabel(0) {}
  void addSymbol(std::unique_ptr<CSymbol> symbol) { symbols[symbol->name] = std::move(symbol); }
  CSymbol *findSymbol(const std::string &name);
  bool nameExists(const std::string &name);
  void enterLoop(uint32_t aContinueLabel, uint32_t aBreakLabel)
    { hasLoop = true; continueLabel = aContinueLabel; breakLabel = aBreakLabel; }
  void exitLoop() { hasLoop = false; }

  std::unordered_map<std::string, std::unique_ptr<CSymbol>> symbols;
  int frameSize;		// current stack depth, relative to the frame pointer

  bool hasLoop;			// true if currently inside a loop in this frame
  uint32_t continueLabel;	// continue label for the current loop
  uint32_t breakLabel;		// break label for the current loop
};

//------------------------------------------------------------------------

class Context {
public:

  Context()
    : topModule(nullptr)
    , vectorHeader(nullptr), setHeader(nullptr), mapHeader(nullptr)
    , intType(nullptr), floatType(nullptr), boolType(nullptr)
    , stringType(nullptr), stringBufType(nullptr)
    , vectorType(nullptr), setType(nullptr), mapType(nullptr)
    , moduleBeingCompiled(nullptr), returnType(nullptr)
    , verbose(false)
  {}

  bool initSearchPath(const std::vector<std::string> &paths);

  void addModule(std::unique_ptr<CModule> module) { modules[module->name] = std::move(module); }
  void addType(std::unique_ptr<CType> type) { types[type->name] = std::move(type); }
  void addConst(std::unique_ptr<CConst> con) { constants[con->name] = std::move(con); }
  void addFunc(std::unique_ptr<CFuncDecl> func) {
    funcs.insert(std::make_pair(func->name, std::move(func)));
  }
  CModule *findModule(const std::string &name);
  CType *findType(const std::string &name);
  CFuncDecl *findFunction(const std::string &name, std::vector<ExprResult> &argResults);

  void pushFrame();
  void popFrame();
  int frameSize();
  void incFrameSize();
  void addSymbol(std::unique_ptr<CSymbol> symbol);
  CSymbol *findSymbol(const std::string &name);
  void enterLoop(uint32_t continueLabel, uint32_t breakLabel);
  void exitLoop();
  Frame *findLoop();

  bool moduleIsVisible(CModule *mod);
  bool typeIsVisible(CType *type);
  bool functionIsVisible(CFuncDecl *func);
  bool constantIsVisible(CConst *con);

  // Returns true if [name] exists as a type or symbol (constant, arg,
  // or var).
  bool nameExists(const std::string &name);

  // module search path
  std::vector<std::string> searchPath;

  // all modules
  std::unordered_map<std::string, std::unique_ptr<CModule>> modules;

  // top module (with main function)
  CModule *topModule;

  // the collection type parameterized module headers
  std::unique_ptr<Module> vectorHeader;
  std::unique_ptr<Module> setHeader;
  std::unique_ptr<Module> mapHeader;

  // all defined types, constants, and functions
  std::unordered_map<std::string, std::unique_ptr<CType>> types;
  std::unordered_map<std::string, std::unique_ptr<CConst>> constants;
  std::unordered_multimap<std::string, std::unique_ptr<CFuncDecl>> funcs;

  // convenience pointers to the builtin types
  CType *intType;
  CType *floatType;
  CType *boolType;
  CType *stringType;
  CType *stringBufType;
  CContainerType *vectorType;
  CContainerType *setType;
  CContainerType *mapType;
  CFuncType *funcType;
  CResultType *resultType;

  // module currently being compiled
  CModule *moduleBeingCompiled;

  // return type of function being compiled
  CTypeRef *returnType;

  // local symbols and loop at current point in function being compiled
  std::vector<std::unique_ptr<Frame>> frames;

  // next available data label
  int nextDataLabel;

  // verbose output flag
  bool verbose;
};

#endif // Context_h
