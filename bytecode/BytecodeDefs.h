//========================================================================
//
// BytecodeDefs.h
//
// Bytecode-related constants.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef BytecodeDefs_h
#define BytecodeDefs_h

#include <string>
#include <unordered_map>

//------------------------------------------------------------------------
// integer limits
//------------------------------------------------------------------------

#define bytecodeMaxInt ( INT64_C(0x007fffffffffffff))
#define bytecodeMinInt (-INT64_C(0x0080000000000000))

//------------------------------------------------------------------------
// opcodes
//------------------------------------------------------------------------

// Map opcode names to opcodes.
extern std::unordered_map<std::string, uint8_t> bcStringToOpcodeMap;

// Map opcodes to opcode names.
extern const char *bcOpcodeToStringMap[256];

#define bcOpcodePushI        0x00
#define bcOpcodePushF        0x01
#define bcOpcodePushTrue     0x02
#define bcOpcodePushFalse    0x03
#define bcOpcodePushBcode    0x04
#define bcOpcodePushData     0x05
#define bcOpcodePushNative   0x06
#define bcOpcodePushNil      0x27
#define bcOpcodePushError    0x28
#define bcOpcodePop          0x07
#define bcOpcodeGetArg       0x08
#define bcOpcodeGetVar       0x09
#define bcOpcodePutVar       0x0a
#define bcOpcodeTestValid    0x29
#define bcOpcodeCheckValid   0x2a
#define bcOpcodeGetStack     0x0b
#define bcOpcodeCall         0x0c
#define bcOpcodePtrcall      0x0d
#define bcOpcodeReturn       0x0e
#define bcOpcodeBranchTrue   0x0f
#define bcOpcodeBranchFalse  0x10
#define bcOpcodeBranch       0x11
#define bcOpcodeLoad         0x12
#define bcOpcodeStore        0x13
#define bcOpcodeAdd          0x14
#define bcOpcodeSub          0x15
#define bcOpcodeMul          0x16
#define bcOpcodeDiv          0x17
#define bcOpcodeMod          0x18
#define bcOpcodeOr           0x19
#define bcOpcodeXor          0x1a
#define bcOpcodeAnd          0x1b
#define bcOpcodeSll          0x1c
#define bcOpcodeSrl          0x1d
#define bcOpcodeSra          0x1e
#define bcOpcodeNeg          0x1f
#define bcOpcodeNot          0x20
#define bcOpcodeCmpeq        0x21
#define bcOpcodeCmpne        0x22
#define bcOpcodeCmplt        0x23
#define bcOpcodeCmpgt        0x24
#define bcOpcodeCmple        0x25
#define bcOpcodeCmpge        0x26

#endif // BytecodeDefs_h
