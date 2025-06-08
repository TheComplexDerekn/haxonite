//========================================================================
//
// BytecodeDefs.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "BytecodeDefs.h"

//------------------------------------------------------------------------
// opcodes
//------------------------------------------------------------------------

std::unordered_map<std::string, uint8_t> bcStringToOpcodeMap {
  { "push.i",        bcOpcodePushI        },
  { "push.f",        bcOpcodePushF        },
  { "push.true",     bcOpcodePushTrue     },
  { "push.false",    bcOpcodePushFalse    },
  { "push.bcode",    bcOpcodePushBcode    },
  { "push.data",     bcOpcodePushData     },
  { "push.native",   bcOpcodePushNative   },
  { "push.nil",      bcOpcodePushNil      },
  { "push.error",    bcOpcodePushError    },
  { "pop",           bcOpcodePop          },
  { "get.arg",       bcOpcodeGetArg       },
  { "get.var",       bcOpcodeGetVar       },
  { "put.var",       bcOpcodePutVar       },
  { "test.valid",    bcOpcodeTestValid    },
  { "check.valid",   bcOpcodeCheckValid   },
  { "get.stack",     bcOpcodeGetStack     },
  { "call",          bcOpcodeCall         },
  { "ptrcall",       bcOpcodePtrcall      },
  { "return",        bcOpcodeReturn       },
  { "branch.true",   bcOpcodeBranchTrue   },
  { "branch.false",  bcOpcodeBranchFalse  },
  { "branch",        bcOpcodeBranch       },
  { "load",          bcOpcodeLoad         },
  { "store",         bcOpcodeStore        },
  { "add",           bcOpcodeAdd          },
  { "sub",           bcOpcodeSub          },
  { "mul",           bcOpcodeMul          },
  { "div",           bcOpcodeDiv          },
  { "mod",           bcOpcodeMod          },
  { "or",            bcOpcodeOr           },
  { "xor",           bcOpcodeXor          },
  { "and",           bcOpcodeAnd          },
  { "sll",           bcOpcodeSll          },
  { "srl",           bcOpcodeSrl          },
  { "sra",           bcOpcodeSra          },
  { "neg",           bcOpcodeNeg          },
  { "not",           bcOpcodeNot          },
  { "cmpeq",         bcOpcodeCmpeq        },
  { "cmpne",         bcOpcodeCmpne        },
  { "cmplt",         bcOpcodeCmplt        },
  { "cmpgt",         bcOpcodeCmpgt        },
  { "cmple",         bcOpcodeCmple        },
  { "cmpge",         bcOpcodeCmpge        }
};

const char *bcOpcodeToStringMap[256] {
  "push.i",
  "push.f",
  "push.true",
  "push.false",
  "push.bcode",
  "push.data",
  "push.native",
  "pop",
  "get.arg",
  "get.var",
  "put.var",
  "get.stack",
  "call",
  "ptrcall",
  "return",
  "branch.true",
  "branch.false",
  "branch",
  "load",
  "store",
  "add",
  "sub",
  "mul",
  "div",
  "mod",
  "or",
  "xor",
  "and",
  "sll",
  "srl",
  "sra",
  "neg",
  "not",
  "cmpeq",
  "cmpne",
  "cmplt",
  "cmpgt",
  "cmple",
  "cmpge",
  "push.nil",
  "push.error",
  "test.valid",
  "check.valid",
  "invalid", "invalid", "invalid", "invalid", "invalid", // 2b-2f
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", // 30-37
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", // 38-3f
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", // 40-47
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", // 48-4f
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", // 50-57
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", // 58-5f
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", // 60-67
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", // 68-6f
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", // 70-77
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", // 78-7f
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", // 80-87
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", // 88-8f
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", // 90-97
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", // 98-9f
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", // a0-a7
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", // a8-af
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", // b0-b7
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", // b8-bf
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", // c0-c7
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", // c8-cf
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", // d0-d7
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", // d8-df
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", // e0-e7
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", // e8-ef
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", // f0-f7
  "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "invalid"  // f8-ff
};
