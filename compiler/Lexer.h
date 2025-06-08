//========================================================================
//
// Lexer.h
//
// Lexical analyzer.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef Lexer_h
#define Lexer_h

#include <deque>
#include <memory>
#include <string>
#include "Location.h"

//------------------------------------------------------------------------

class Token {
public:

  enum class Kind {
    keywordBreak,
    keywordCase,
    keywordConst,
    keywordContinue,
    keywordDefault,
    keywordDo,
    keywordEnd,
    keywordElse,
    keywordElseif,
    keywordEnum,
    keywordError,
    keywordFalse,
    keywordFor,
    keywordFunc,
    keywordHeader,
    keywordIf,
    keywordImport,
    keywordIs,
    keywordMake,
    keywordModule,
    keywordNativefunc,
    keywordNativetype,
    keywordNew,
    keywordNil,
    keywordOk,
    keywordReturn,
    keywordStruct,
    keywordSubstruct,
    keywordThen,
    keywordTrue,
    keywordTypematch,
    keywordValid,
    keywordVar,
    keywordVarstruct,
    keywordWhile,

    puncAmpersand,
    puncAmpersandAmpersand,
    puncArrowR,
    puncAsterisk,
    puncBar,
    puncBarBar,
    puncBraceL,
    puncBraceR,
    puncBracketL,
    puncBracketR,
    puncCaret,
    puncColon,
    puncComma,
    puncDollar,
    puncEq,
    puncEqEq,
    puncEqEqEq,
    puncExclam,
    puncExclamEq,
    puncExclamEqEq,
    puncGt,
    puncGtEq,
    puncGtGt,
    puncLt,
    puncLtEq,
    puncLtLt,
    puncMinus,
    puncParenL,
    puncParenR,
    puncPercent,
    puncPeriod,
    puncPeriodPeriod,
    puncPlus,
    puncQuestion,
    puncSemicolon,
    puncSharp,
    puncSlash,

    ident,

    decimalIntLiteral,
    binaryIntLiteral,
    octalIntLiteral,
    hexIntLiteral,
    floatLiteral,
    charLiteral,
    stringLiteral,
    interpString,

    error,
    eof
  };

  Token(Kind aKind, const std::string &aStr, Location aLoc)
    : mKind(aKind), mStr(aStr), mLoc(aLoc) {}

  Kind kind() { return mKind; }
  std::string kindName();
  static std::string kindName(Kind aKind);
  bool is(Kind aKind) { return mKind == aKind; }
  std::string str() { return mStr; }
  Location loc() { return mLoc; }

private:

  Kind mKind;
  std::string mStr;
  Location mLoc;
};

//------------------------------------------------------------------------

class Lexer {
public:

  // Create a lexer with input data [aInput]. [aPath] is used as the
  // file name in Locations.
  Lexer(const std::string &aInput, const std::string &aPath);

  // Get the [idx]th next token.  get(0) returns the next token;
  // get(1) looks ahead one token; etc.
  Token get(size_t idx);

  // Shifts one token out.
  void shift();

  // Returns true if there are more tokens left, i.e., if the next
  // token is not EOF.
  bool moreInput();

private:

  Token nextToken();
  void skipWhitespaceAndComments();
  Token lexNumLiteral(char c0, char c1);
  Token lexCharLiteral();
  Token lexStringLiteral();
  Token lexInterpString();
  Token lexIdentOrKeyword();
  Token lexPunc();
  bool prefixMatch(const std::string &s);

  std::string mInput;
  size_t mPos;			// current position in mInput
  size_t mLine;			// line number of mInput[mPos]
  std::shared_ptr<std::string> mPath;
  std::deque<Token> mTokenBuf;
};

#endif // Lexer_h
