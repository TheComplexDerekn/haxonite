//========================================================================
//
// CodeGenBlock.h
//
// Generate bytecode for blocks.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef CodeGenBlock_h
#define CodeGenBlock_h

#include "AST.h"
#include "BytecodeFile.h"
#include "Context.h"

//------------------------------------------------------------------------

// Result of generating code for a block.
//
// On error: ok=false.
// On success: ok=true; fallthrough=true if there is at least one path
// that falls through the end of the block, or false if all paths end
// with return.
struct BlockResult {
  BlockResult(): ok(false) {}
  explicit BlockResult(bool aFallthrough): ok(true), fallthrough(aFallthrough) {}

  bool ok;
  bool fallthrough;
};

//------------------------------------------------------------------------

// Generate bytecode for [block] and add it to [bcFunc].
extern BlockResult codeGenBlock(Block *block, Context &ctx, BytecodeFile &bcFunc);

#endif // CodeGenBlock_h
