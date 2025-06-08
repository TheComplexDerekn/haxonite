//========================================================================
//
// runtime_system.h
//
// Runtime library: system functions.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef runtime_system_h
#define runtime_system_h

#include "BytecodeEngine.h"

extern void runtime_system_init(BytecodeEngine &engine);

extern void setCommandLineArgs(int argc, char *argv[], BytecodeEngine &engine);

#endif // runtime_system_h
