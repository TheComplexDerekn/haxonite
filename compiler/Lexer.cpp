//========================================================================
//
// Lexer.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "Lexer.h"
#include <unordered_map>
#include <utility>
#include <vector>

//------------------------------------------------------------------------

static const char *tokenKindNames[] = {
  "break",
  "case",
  "const",
  "continue",
  "default",
  "do",
  "end",
  "else",
  "elseif",
  "enum",
  "error",
  "false",
  "for",
  "func",
  "header",
  "if",
  "import",
  "is",
  "make",
  "module",
  "nativefunc",
  "nativetype",
  "new",
  "nil",
  "ok",
  "public",
  "return",
  "struct",
  "substruct",
  "then",
  "true",
  "typematch",
  "valid",
  "var",
  "varstruct",
  "while",

  "&",
  "&&",
  "->",
  "*",
  "|",
  "||",
  "{",
  "}",
  "[",
  "]",
  "^",
  ":",
  ",",
  "$",
  "=",
  "==",
  "===",
  "!",
  "!=",
  "!==",
  ">",
  ">=",
  ">>",
  "<",
  "<=",
  "<<",
  "-",
  "(",
  ")",
  "%",
  ".",
  "..",
  "+",
  "?",
  ";",
  "#",
  "/",

  "identifier",

  "decimal integer literal",
  "binary integer literal",
  "octal integer literal",
  "hex integer literal",
  "floating point literal",
  "character literal",
  "string literal",
  "interpolated string",

  "error",
  "end-of-file"
};

static std::unordered_map<std::string, Token::Kind> keywords {
  { "break",      Token::Kind::keywordBreak      },
  { "case",       Token::Kind::keywordCase       },
  { "const",      Token::Kind::keywordConst      },
  { "continue",   Token::Kind::keywordContinue   },
  { "default",    Token::Kind::keywordDefault    },
  { "do",         Token::Kind::keywordDo         },
  { "end",        Token::Kind::keywordEnd        },
  { "else",       Token::Kind::keywordElse       },
  { "elseif",     Token::Kind::keywordElseif     },
  { "enum",       Token::Kind::keywordEnum       },
  { "error",      Token::Kind::keywordError      },
  { "false",      Token::Kind::keywordFalse      },
  { "for",        Token::Kind::keywordFor        },
  { "func",       Token::Kind::keywordFunc       },
  { "header",     Token::Kind::keywordHeader     },
  { "if",         Token::Kind::keywordIf         },
  { "import",     Token::Kind::keywordImport     },
  { "is",         Token::Kind::keywordIs         },
  { "make",       Token::Kind::keywordMake       },
  { "module",     Token::Kind::keywordModule     },
  { "nativefunc", Token::Kind::keywordNativefunc },
  { "nativetype", Token::Kind::keywordNativetype },
  { "new",        Token::Kind::keywordNew        },
  { "nil",        Token::Kind::keywordNil        },
  { "ok",         Token::Kind::keywordOk         },
  { "public",     Token::Kind::keywordPublic     },
  { "return",     Token::Kind::keywordReturn     },
  { "struct",     Token::Kind::keywordStruct     },
  { "substruct",  Token::Kind::keywordSubstruct  },
  { "then",       Token::Kind::keywordThen       },
  { "true",       Token::Kind::keywordTrue       },
  { "typematch",  Token::Kind::keywordTypematch  },
  { "valid",      Token::Kind::keywordValid      },
  { "var",        Token::Kind::keywordVar        },
  { "varstruct",  Token::Kind::keywordVarstruct  },
  { "while",      Token::Kind::keywordWhile      }
};

// NB: longer strings need to come first, e.g., "<<" before "<".
static std::unordered_map<char, std::vector<std::pair<std::string, Token::Kind>>> punctuation {
  {'&', {{"&&",  Token::Kind::puncAmpersandAmpersand},
         {"&",   Token::Kind::puncAmpersand}}},
  {'*', {{"*",   Token::Kind::puncAsterisk}}},
  {'|', {{"||",  Token::Kind::puncBarBar},
	 {"|",   Token::Kind::puncBar}}},
  {'{', {{"{",   Token::Kind::puncBraceL}}},
  {'}', {{"}",   Token::Kind::puncBraceR}}},
  {'[', {{"[",   Token::Kind::puncBracketL}}},
  {']', {{"]",   Token::Kind::puncBracketR}}},
  {'^', {{"^",   Token::Kind::puncCaret}}},
  {':', {{":",   Token::Kind::puncColon}}},
  {',', {{",",   Token::Kind::puncComma}}},
  {'$', {{"$",   Token::Kind::puncDollar}}},
  {'=', {{"===", Token::Kind::puncEqEqEq},
         {"==",  Token::Kind::puncEqEq},
	 {"=",   Token::Kind::puncEq}}},
  {'!', {{"!==", Token::Kind::puncExclamEqEq},
         {"!=",  Token::Kind::puncExclamEq},
	 {"!",   Token::Kind::puncExclam}}},
  {'>', {{">=",  Token::Kind::puncGtEq},
	 {">>",  Token::Kind::puncGtGt},
	 {">",   Token::Kind::puncGt}}},
  {'<', {{"<=",  Token::Kind::puncLtEq},
	 {"<<",  Token::Kind::puncLtLt},
	 {"<",   Token::Kind::puncLt}}},
  {'-', {{"->",  Token::Kind::puncArrowR},
         {"-",   Token::Kind::puncMinus}}},
  {'(', {{"(",   Token::Kind::puncParenL}}},
  {')', {{")",   Token::Kind::puncParenR}}},
  {'%', {{"%",   Token::Kind::puncPercent}}},
  {'.', {{"..",  Token::Kind::puncPeriodPeriod},
         {".",   Token::Kind::puncPeriod}}},
  {'+', {{"+",   Token::Kind::puncPlus}}},
  {'?', {{"?",   Token::Kind::puncQuestion}}},
  {';', {{";",   Token::Kind::puncSemicolon}}},
  {'#', {{"#",   Token::Kind::puncSharp}}},
  {'/', {{"/",   Token::Kind::puncSlash}}}
};

static const char *blockCommentBegin = "/*";
static int blockCommentBeginLength = 2;
static const char *blockCommentEnd   = "*/";
static int blockCommentEndLength = 2;
static const char *lineCommentBegin  = "//";
static int lineCommentBeginLength = 2;

//------------------------------------------------------------------------

std::string Token::kindName() {
  return tokenKindNames[(int)mKind];
}

std::string Token::kindName(Kind aKind) {
  return tokenKindNames[(int)aKind];
}

//------------------------------------------------------------------------

Lexer::Lexer(const std::string &aInput, const std::string &aPath) {
  mInput = aInput;
  mPos = 0;
  mLine = 1;
  mPath = std::make_shared<std::string>(aPath);
}

Token Lexer::get(size_t idx) {
  while (mTokenBuf.size() <= idx) {
    mTokenBuf.push_back(nextToken());
  }
  return mTokenBuf[idx];
}

void Lexer::shift() {
  if (mTokenBuf.empty()) {
    nextToken();
  } else {
    mTokenBuf.pop_front();
  }
}

bool Lexer::moreInput() {
  return !get(0).is(Token::Kind::eof);
}

Token Lexer::nextToken() {
  skipWhitespaceAndComments();
  if (mPos >= mInput.size()) {
    return Token(Token::Kind::eof, "", Location(mPath, mLine));
  }
  char c0 = mInput[mPos];
  char c1 = '\0';
  if (mPos+1 < mInput.size()) {
    c1 = mInput[mPos+1];
  }
  if (c0 >= '0' && c0 <= '9') {
    return lexNumLiteral(c0, c1);
  } else if (c0 == '\'') {
    return lexCharLiteral();
  } else if (c0 =='"') {
    return lexStringLiteral();
  } else if (c0 == '$' && c1 == '"') {
    return lexInterpString();
  } else if ((c0 >= 'A' && c0 <= 'Z') || (c0 >= 'a' && c0 <= 'z')) {
    return lexIdentOrKeyword();
  } else {
    return lexPunc();
  }
}

void Lexer::skipWhitespaceAndComments() {
  bool inBlockComment = false;
  bool inLineComment = false;
  while (true) {
    if (mPos >= mInput.size()) {
      return;
    }
    char c = mInput[mPos];
    if (c == '\n') {
      inLineComment = false;
      ++mPos;
      ++mLine;
    } else if (inBlockComment) {
      if (prefixMatch(blockCommentEnd)) {
	inBlockComment = false;
	mPos += blockCommentEndLength;
      } else {
	++mPos;
      }
    } else if (inLineComment) {
      ++mPos;
    } else {
      if (prefixMatch(blockCommentBegin)) {
	inBlockComment = true;
	mPos += blockCommentBeginLength;
      } else if (prefixMatch(lineCommentBegin)) {
	inLineComment = true;
	mPos += lineCommentBeginLength;
      } else if (c == ' ' || c == '\t') {
	++mPos;
      } else {
	return;
      }
    }
  }
}

Token Lexer::lexNumLiteral(char c0, char c1) {
  Location loc(mPath, mLine);

  //--- check for 0b/0o/0x prefix
  int radix = 10;
  Token::Kind kind = Token::Kind::decimalIntLiteral;
  char cLow0 = '0', cHigh0 = '9';
  char cLow1 = '1', cHigh1 = '0';
  char cLow2 = '1', cHigh2 = '0';
  if (c0 == '0') {
    if (c1 == 'b') {
      radix = 2;
      kind = Token::Kind::binaryIntLiteral;
      mPos += 2;
    } else if (c1 == 'o') {
      radix = 8;
      kind = Token::Kind::octalIntLiteral;
      cLow0 = '0';
      cHigh0 = '7';
      mPos += 2;
    } else if (c1 == 'x') {
      radix = 16;
      kind = Token::Kind::hexIntLiteral;
      cLow0 = '0';
      cHigh0 = '9';
      cLow1 = 'A';
      cHigh1 = 'F';
      cLow2 = 'a';
      cHigh2 = 'f';
      mPos += 2;
    }
  }

  //--- check that there is at least one digit
  if (!(mPos < mInput.size() &&
	((mInput[mPos] >= cLow0 && mInput[mPos] <= cHigh0) ||
	 (mInput[mPos] >= cLow1 && mInput[mPos] <= cHigh1) ||
	 (mInput[mPos] >= cLow2 && mInput[mPos] <= cHigh2)))) {
    return Token(Token::Kind::error, std::string("Invalid numeric literal"), loc);
  }

  //--- consume one or more digits
  size_t i0 = mPos;
  char c;
  do {
    ++mPos;
    if (mPos >= mInput.size()) {
      break;
    }
    c = mInput[mPos];
  } while ((c >= cLow0 && c <= cHigh0) ||
	   (c >= cLow1 && c <= cHigh1) ||
	   (c >= cLow2 && c <= cHigh2));

  //--- check for floating point
  if (radix == 10 && mPos < mInput.size() && (mInput[mPos] == '.' ||
					      mInput[mPos] == 'e' ||
					      mInput[mPos] == 'E')) {
    if (mInput[mPos] == '.') {
      ++mPos;
      if (!(mPos < mInput.size() && mInput[mPos] >= '0' && mInput[mPos] <= '9')) {
	return Token(Token::Kind::error, std::string("Invalid floating point literal"), loc);
      }
      do {
	++mPos;
	if (mPos >= mInput.size()) {
	  break;
	}
	c = mInput[mPos];
      } while (c >= '0' && c <= '9');
    }
    if (mPos < mInput.size() && (mInput[mPos] == 'e' || mInput[mPos] == 'E')) {
      ++mPos;
      if (mPos < mInput.size() && (mInput[mPos] == '+' || mInput[mPos] == '-')) {
	++mPos;
      }
      if (!(mPos < mInput.size() && mInput[mPos] >= '0' && mInput[mPos] <= '9')) {
	return Token(Token::Kind::error, std::string("Invalid floating point literal"), loc);
      }
      do {
	++mPos;
	if (mPos >= mInput.size()) {
	  break;
	}
	c = mInput[mPos];
      } while (c >= '0' && c <= '9');
    }
    return Token(Token::Kind::floatLiteral, mInput.substr(i0, mPos - i0), loc);

  //--- create an integer literal token
  } else {
    return Token(kind, mInput.substr(i0, mPos - i0), loc);
  }
}

Token Lexer::lexCharLiteral() {
  Location loc(mPath, mLine);
  ++mPos;  // skip '
  std::string s;
  while (true) {
    if (mPos >= mInput.size()) {
      return Token(Token::Kind::error, "End of input in character literal", loc);
    }
    char c = mInput[mPos];
    if (c == '\n') {
      return Token(Token::Kind::error, "End of line in character literal", loc);
    }
    ++mPos;
    if (c == '\'') {
      break;
    }
    if (c == '\\') {
      if (mPos >= mInput.size()) {
	return Token(Token::Kind::error, "End of input in character literal", loc);
      }
      c = mInput[mPos];
      if (c == '\n') {
	return Token(Token::Kind::error, "End of line in character literal", loc);
      }
      ++mPos;
      switch (c) {
      case 'n':
	c = '\n';
	break;
      case 'r':
	c = '\r';
	break;
      case 't':
	c = '\t';
	break;
      case '\'':
      case '\\':
	break;
      default:
	return Token(Token::Kind::error, "Invalid escape sequence in character literal", loc);
      }
    }
    s.push_back(c);
  }
  if (s.size() == 0) {
    return Token(Token::Kind::error, "Invalid empty character literal", loc);
  }
  if (s.size() > 1) {
    return Token(Token::Kind::error, "Invalid multi-character literal", loc);
  }
  return Token(Token::Kind::charLiteral, s, loc);
}

Token Lexer::lexStringLiteral() {
  Location loc(mPath, mLine);
  ++mPos;  // skip "
  std::string s;
  while (true) {
    if (mPos >= mInput.size()) {
      return Token(Token::Kind::error, "End of input in string literal", loc);
    }
    char c = mInput[mPos];
    if (c == '\n') {
      return Token(Token::Kind::error, "End of line in string literal", loc);
    }
    ++mPos;
    if (c == '"') {
      break;
    }
    if (c == '\\') {
      if (mPos >= mInput.size()) {
	return Token(Token::Kind::error, "End of input in string literal", loc);
      }
      c = mInput[mPos];
      if (c == '\n') {
	return Token(Token::Kind::error, "End of line in string literal", loc);
      }
      ++mPos;
      switch (c) {
      case 'n':
	c = '\n';
	break;
      case 'r':
	c = '\r';
	break;
      case 't':
	c = '\t';
	break;
      case '"':
      case '\\':
	break;
      default:
	return Token(Token::Kind::error, "Invalid escape sequence in string literal", loc);
      }
    }
    s.push_back(c);
  }
  return Token(Token::Kind::stringLiteral, s, loc);
}

Token Lexer::lexInterpString() {
  Location loc(mPath, mLine);
  mPos += 2;  // skip $"
  std::string s;
  while (true) {
    if (mPos >= mInput.size()) {
      return Token(Token::Kind::error, "End of input in interpolated string", loc);
    }
    char c = mInput[mPos];
    if (c == '\n') {
      return Token(Token::Kind::error, "End of line in interpolated string", loc);
    }
    ++mPos;
    if (c == '"') {
      break;
    }
    // escape chars in interpolated strings have to be handled later
    // (becuase braces can be escaped) -- so we just skip one
    // character here, to allow for \"
    if (c == '\\') {
      s.push_back(c);
      if (mPos >= mInput.size()) {
	return Token(Token::Kind::error, "End of input in interpolated string", loc);
      }
      c = mInput[mPos];
      if (c == '\n') {
	return Token(Token::Kind::error, "End of line in interpolated string", loc);
      }
      ++mPos;
    }
    s.push_back(c);
  }
  return Token(Token::Kind::interpString, s, loc);
}

Token Lexer::lexIdentOrKeyword() {
  Location loc(mPath, mLine);

  size_t i0 = mPos;
  char c;
  do {
    ++mPos;
    if (mPos >= mInput.size()) {
      break;
    }
    c = mInput[mPos];
  } while ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'));
  std::string s = mInput.substr(i0, mPos - i0);

  auto iter = keywords.find(s);
  if (iter != keywords.end()) {
    return Token(iter->second, s, loc);
  }

  return Token(Token::Kind::ident, s, loc);
}

Token Lexer::lexPunc() {
  Location loc(mPath, mLine);
  size_t i0 = mPos;
  char c0 = mInput[mPos] & 0xff;
  auto iter = punctuation.find(c0);
  if (iter != punctuation.end()) {
    for (auto pair : iter->second) {
      if (prefixMatch(pair.first)) {
	mPos += pair.first.size();
	return Token(pair.second, mInput.substr(i0, mPos - i0), loc);
      }
    }
  }
  ++mPos; // avoid an infinite loop
  return Token(Token::Kind::error,
	       std::string("Unknown token '") + mInput.substr(i0, 1) + "'",
	       loc);
}

bool Lexer::prefixMatch(const std::string &s) {
  return mInput.compare(mPos, s.size(), s) == 0;
}
