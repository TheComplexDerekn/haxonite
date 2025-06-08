//========================================================================
//
// FunctionChecker.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "FunctionChecker.h"
#include "Error.h"
#include "TypeCheck.h"

//------------------------------------------------------------------------

bool checkFunctions(Context &ctx) {
  bool ok = true;

  //--- check for duplicate functions
  auto iter1 = ctx.funcs.begin();
  while (iter1 != ctx.funcs.end()) {
    auto next = iter1;
    ++next;
    for (auto iter2 = next;
	 iter2 != ctx.funcs.end() && iter2->first == iter1->first;
	 ++iter2) {
      if (functionCollision(iter1->second.get(), iter2->second.get())) {
	error(iter1->second->loc, "Duplicate defintion of function '%s'",
	      iter1->second->name.c_str());
	ok = false;
      }
    }
    iter1 = next;
  }

  //--- check for a main() function
  CFuncDecl *mainFunc = nullptr;
  auto range = ctx.funcs.equal_range("main");
  for (auto iter = range.first; iter != range.second; ++iter) {
    CFuncDecl *funcDecl = iter->second.get();
    if (funcDecl->args.size() == 0) {
      mainFunc = funcDecl;
      break;
    }
  }
  if (!mainFunc) {
    error(Location(), "No definition of the main() function");
    ok = false;
  }
  if (mainFunc->module != ctx.topModule) {
    error(mainFunc->loc, "main() function is not defined in the top module");
    ok = false;
  }

  return ok;
}
