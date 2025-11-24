//========================================================================
//
// Parser.h
//
// Parser - builds the AST for a module or header.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef Parser_h
#define Parser_h

#include <memory>
#include <string>
#include "AST.h"
#include "Lexer.h"

//------------------------------------------------------------------------

class Parser {
public:

  // Create a parser with input data [aInput]. [aPath] is used as the
  // file name in Locations.
  Parser(const std::string &aInput, const std::string &aPath)
    : lexer(aInput, aPath)
  {}

  // Parse a module. Returns null on error.
  std::unique_ptr<Module> parseModule();

  // Parse a header. Returns null on error.
  std::unique_ptr<Module> parseHeader();

private:

  std::unique_ptr<Import> parseImport();
  std::unique_ptr<ModuleElem> parseModuleElem();
  std::unique_ptr<ModuleElem> parseHeaderElem();
  std::unique_ptr<StructDefn> parseStructDefn(bool pub);
  std::unique_ptr<VarStructDefn> parseVarStructDefn(bool pub);
  std::unique_ptr<SubStructDefn> parseSubStructDefn();
  std::unique_ptr<Field> parseField();
  std::unique_ptr<EnumDefn> parseEnumDefn(bool pub);
  std::unique_ptr<NativeTypeDefn> parseNativeTypeDefn(bool pub);
  std::unique_ptr<ConstDefn> parseConstDefn(bool pub);
  std::unique_ptr<FuncDefn> parseFuncDefn(bool isDecl, bool pub);
  std::unique_ptr<Arg> parseArg();
  std::unique_ptr<TypeRef> parseTypeRef();
  std::unique_ptr<Block> parseBlock(Token::Kind end1,
				    Token::Kind end2 = Token::Kind::error,
				    Token::Kind end3 = Token::Kind::error);
  std::unique_ptr<Stmt> parseStmt();
  std::unique_ptr<VarStmt> parseVarStmt();
  std::unique_ptr<IfStmt> parseIfStmt();
  std::unique_ptr<WhileStmt> parseWhileStmt();
  std::unique_ptr<ForStmt> parseForStmt();
  std::unique_ptr<BreakStmt> parseBreakStmt();
  std::unique_ptr<ContinueStmt> parseContinueStmt();
  std::unique_ptr<TypematchStmt> parseTypematchStmt();
  std::unique_ptr<TypematchCase> parseTypematchCase();
  std::unique_ptr<ReturnStmt> parseReturnStmt();
  std::unique_ptr<Stmt> parseExprOrAssignStmt();
  std::unique_ptr<Expr> parseExpr();
  std::unique_ptr<Expr> parseCondOrExpr();
  std::unique_ptr<Expr> parseCondAndExpr();
  std::unique_ptr<Expr> parseOrExpr();
  std::unique_ptr<Expr> parseAndExpr();
  std::unique_ptr<Expr> parseEqualExpr();
  std::unique_ptr<Expr> parseCmpExpr();
  std::unique_ptr<Expr> parseShiftExpr();
  std::unique_ptr<Expr> parseAddExpr();
  std::unique_ptr<Expr> parseMulExpr();
  std::unique_ptr<Expr> parseUnOpExpr();
  std::unique_ptr<Expr> parseSuffixExpr();
  std::unique_ptr<Expr> parsePostfixExpr();
  std::unique_ptr<Expr> parseCallExpr(std::unique_ptr<Expr> func);
  std::unique_ptr<Expr> parseMemberExpr(std::unique_ptr<Expr> lhs);
  std::unique_ptr<Expr> parseIndexExpr(std::unique_ptr<Expr> obj);
  std::unique_ptr<Expr> parseFactorExpr();
  std::unique_ptr<Expr> parseParenExpr();
  std::unique_ptr<Expr> parseNewExpr();
  std::unique_ptr<Expr> parseMakeExpr();
  std::unique_ptr<FieldInit> parseFieldInit();
  std::unique_ptr<Expr> parseFuncPointerExpr();
  std::unique_ptr<Expr> parseNilExpr();
  std::unique_ptr<Expr> parseErrorExpr();
  std::unique_ptr<Expr> parseValidExpr();
  std::unique_ptr<Expr> parseOkExpr();
  std::unique_ptr<Expr> parseLitVectorExpr();
  std::unique_ptr<Expr> parseLitSetOrMapExpr();
  std::unique_ptr<Expr> parseInterpStringExpr();

  struct ParseBinaryOpInfo {
    Token::Kind token;
    BinaryOp op;
  };
  std::unique_ptr<Expr> parseBinary(std::unique_ptr<Expr> (Parser::*parseChild)(),
				    const std::vector<ParseBinaryOpInfo> &opInfoVec);
  std::unique_ptr<Expr> parseLeftBinary(std::unique_ptr<Expr> (Parser::*parseChild)(),
					const std::vector<ParseBinaryOpInfo> &opInfoVec);

  struct ParseUnaryOpInfo {
    Token::Kind token;
    UnaryOp op;
  };
  std::unique_ptr<Expr> parseLeftUnary(std::unique_ptr<Expr> (Parser::*parseChild)(),
				       const std::vector<ParseUnaryOpInfo> &opInfoVec);

  template<class Elem>
  bool parseList(std::unique_ptr<Elem> (Parser::*parseElem)(),
		 Token::Kind end, bool allowEmpty,
		 std::vector<std::unique_ptr<Elem>> &v);

  template<class Elem>
  bool parseSepList(std::unique_ptr<Elem> (Parser::*parseElem)(),
		    Token::Kind separator, Token::Kind end,
		    bool allowEmpty,
		    std::vector<std::unique_ptr<Elem>> &v);

  bool expect(Token::Kind kind);
  bool expect(Token::Kind kind, std::string &str);

  Lexer lexer;
};

#endif // Parser_h
