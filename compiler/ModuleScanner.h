//========================================================================
//
// ModuleScanner.h
//
// Scan modules, starting with the top-level module, and following the
// imports.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef ModuleScanner_h
#define ModuleScanner_h

#include <string>
#include "Context.h"

//------------------------------------------------------------------------

// Scan the builtin (runtime library) module, [moduleName]. Adds types
// and functions to [ctx]. Returns true on success, or false if any
// errors were detected.
extern bool scanBuiltinModule(const std::string &moduleName, Context &ctx);

// Scan the builtin container type headers. Adds parsed modules to
// [ctx].  Returns true on success, or false if any errors were
// detected.
extern bool scanContainerTypeHeaders(const std::string &vectorHeaderName,
				     const std::string &setHeaderName,
				     const std::string &mapHeaderName,
				     Context &ctx);

// Scan the module tree, starting with [topModuleName] and recursively
// following imports. Adds types and functions to [ctx]. Returns true
// on success, or false if any errors were detected.
extern bool scanModules(const std::string &topModuleName, Context &ctx);

#endif // ModuleScanner_h
