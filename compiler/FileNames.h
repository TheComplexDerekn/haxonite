//========================================================================
//
// FileNames.h
//
// Generate directory and file names.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef FileNames_h
#define FileNames_h

#include <string>

//------------------------------------------------------------------------

// Construct the source directory path under module directory [dir].
extern std::string makeSourceDirPath(const std::string &dir);

// Construct the header directory path under module directory [dir].
extern std::string makeHeaderDirPath(const std::string &dir);

// Construct the object directory path under module directory [dir].
extern std::string makeObjectDirPath(const std::string &dir);

// Construct the bin directory path under module directory [dir].
extern std::string makeBinDirPath(const std::string &dir);

// Construct a module source file name, given the module directory and
// module name.
extern std::string makeSourceFileName(const std::string &dir, const std::string &moduleName);

// Construct a module header file name, given the module directory and
// module name.
extern std::string makeHeaderFileName(const std::string &dir, const std::string &moduleName);

// Construct a short source or header file name, suitable for use in
// error messages, given the module directory and module name.
extern std::string makeShortFileName(const std::string &dir, const std::string &moduleName,
				     bool isHeader);

// Construct a module object file name, given the module directory and
// module name.
extern std::string makeObjectFileName(const std::string &dir, const std::string &moduleName);

// Construct an executable file name, given the module directory and
// module name.
extern std::string makeExecutableFileName(const std::string &dir, const std::string &moduleName);

#endif // FileNames_h
