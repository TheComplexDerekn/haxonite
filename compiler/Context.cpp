//========================================================================
//
// Context.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "Context.h"
#include "SysIO.h"
#include "TypeCheck.h"

//------------------------------------------------------------------------

CSymbol *Frame::findSymbol(const std::string &name) {
  auto iter = symbols.find(name);
  if (iter == symbols.end()) {
    return nullptr;
  }
  return iter->second.get();
}

bool Frame::nameExists(const std::string &name) {
  return symbols.count(name) > 0;
}

//------------------------------------------------------------------------

bool Context::initSearchPath(const std::vector<std::string> &paths) {
  for (const std::string &p : paths) {
    searchPath.push_back(p);
  }

  std::string s = getEnvVar("HAXONITEPATH");
  size_t i = 0;
  while (i < s.size()) {
    size_t j = s.find(':', i);
    if (j == std::string::npos) {
      searchPath.push_back(s.substr(i));
      break;
    }
    searchPath.push_back(s.substr(i, j - i));
    i = j + 1;
  }

  return !searchPath.empty();
}

CModule *Context::findModule(const std::string &name) {
  auto iter = modules.find(name);
  if (iter == modules.end()) {
    return nullptr;
  }
  return iter->second.get();
}

CType *Context::findType(const std::string &name) {
  auto iter = types.find(name);
  if (iter == types.end()) {
    return nullptr;
  }
  CType *type = iter->second.get();
  if (!moduleIsVisible(type->module)) {
    return nullptr;
  }
  return type;
}

CFuncDecl *Context::findFunction(const std::string &name, std::vector<ExprResult> &argResults) {
  auto range = funcs.equal_range(name);
  for (auto iter = range.first; iter != range.second; ++iter) {
    CFuncDecl *funcDecl = iter->second.get();
    if (functionMatch(argResults, funcDecl)) {
      if (!moduleIsVisible(funcDecl->module)) {
	return nullptr;
      }
      for (std::unique_ptr<CArg> &arg : funcDecl->args) {
	if (!moduleIsVisible(arg->type->type->module)) {
	  return nullptr;
	}
      }
      if (funcDecl->returnType &&
	  !moduleIsVisible(funcDecl->returnType->type->module)) {
	return nullptr;
      }
      return funcDecl;
    }
  }
  return nullptr;
}

bool Context::moduleIsVisible(CModule *mod) {
  if (mod->builtin || mod == moduleBeingCompiled) {
    return true;
  }
  for (CModule *import : moduleBeingCompiled->imports) {
    if (mod == import) {
      return true;
    }
  }
  return false;
}

void Context::pushFrame() {
  int newFrameSize = frames.empty() ? 0 : frameSize();
  frames.push_back(std::make_unique<Frame>(newFrameSize));
}

void Context::popFrame() {
  frames.pop_back();
}

int Context::frameSize() {
  return frames.back()->frameSize;
}

void Context::incFrameSize() {
  ++frames.back()->frameSize;
}

void Context::addSymbol(std::unique_ptr<CSymbol> symbol) {
  frames.back()->addSymbol(std::move(symbol));
}

CSymbol *Context::findSymbol(const std::string &name) {
  for (size_t i = frames.size(); i > 0; --i) {
    CSymbol *symbol = frames[i-1]->findSymbol(name);
    if (symbol) {
      return symbol;
    }
  }

  auto iter = constants.find(name);
  if (iter != constants.end()) {
    CConst *con = iter->second.get();
    if (!moduleIsVisible(con->module)) {
      return nullptr;
    }
    return con;
  }

  return nullptr;
}

void Context::enterLoop(uint32_t continueLabel, uint32_t breakLabel) {
  frames.back()->enterLoop(continueLabel, breakLabel);
}

void Context::exitLoop() {
  frames.back()->exitLoop();
}

Frame *Context::findLoop() {
  for (size_t i = frames.size(); i > 0; --i) {
    if (frames[i-1]->hasLoop) {
      return frames[i-1].get();
    }
  }
  return nullptr;
}

bool Context::nameExists(const std::string &name) {
  if (types.count(name) > 0 || constants.count(name) > 0) {
    return true;
  }
  for (size_t i = frames.size(); i > 0; --i) {
    if (frames[i-1]->nameExists(name)) {
      return true;
    }
  }
  return false;
}
