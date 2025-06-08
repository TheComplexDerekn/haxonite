//========================================================================
//
// SysIO.h
//
// Platform-dependent I/O functions.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef SysIO_h
#define SysIO_h

#include <string>
#include <vector>
#include "DateTime.h"

//------------------------------------------------------------------------

// Read the environment variable [var] and return its value.
// Returns an empty string if the variable is unset.
extern std::string getEnvVar(const std::string &var);

// Set the environment variable [var] to [value].
extern void setEnvVar(const std::string &var, const std::string &value);

// Return the standard path for config files.
extern std::string configDir();

// Returns true if [path] refers to a regular file.
extern bool pathIsFile(const std::string &path);

// Returns the modification time of [path].  Returns an invalid
// DateTime on error (e.g., if [path] doesn't exist).
extern DateTime pathModTime(const std::string &path);

// Reads the contents of a file at [path] into [contents].
// Returns true on success, false on error.
extern bool readFile(const std::string &path, std::string &contents);

// Create a directory at [path]. Returns true on sucess, false on
// error.
extern bool createDir(const std::string &path);

// Run a command. [cmd] is the argv vector. The first element (cmd[0])
// is the command to run -- the shell search path is used to find it.
// On success: sets [exitStatus] to the process exit status and
// returns true. On failure: returns false.
extern bool run(std::vector<std::string> cmd, int &exitStatus);

#endif // SysIO_h
