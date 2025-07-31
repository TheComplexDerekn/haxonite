//========================================================================
//
// Parser.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "Parser.h"
#include "Error.h"

//------------------------------------------------------------------------

std::unique_ptr<Module> Parser::parseModule() {
  Location loc = lexer.get(0).loc();

  if (!expect(Token::Kind::keywordModule)) {
    error(lexer.get(0).loc(), "Expected 'module' declaration");
    return nullptr;
  }
  std::string name;
  if (!expect(Token::Kind::ident, name)) {
    error(lexer.get(0).loc(), "Expected module name");
    return nullptr;
  }
  if (!expect(Token::Kind::keywordIs)) {
    error(lexer.get(0).loc(), "Expected 'is' after module name");
    return nullptr;
  }

  bool ok = true;

  std::vector<std::unique_ptr<Import>> imports;
  while (lexer.get(0).is(Token::Kind::keywordImport)) {
    std::unique_ptr<Import> import = parseImport();
    if (import) {
      imports.push_back(std::move(import));
    } else {
      ok = false;
    }
  }

  std::vector<std::unique_ptr<ModuleElem>> elems;
  if (!parseList(&Parser::parseModuleElem, Token::Kind::keywordEnd, true, elems)) {
    ok = false;
  }

  if (!expect(Token::Kind::keywordEnd)) {
    error(lexer.get(0).loc(), "Missing 'end' in module");
    ok = false;
  }
  if (lexer.moreInput()) {
    error(lexer.get(0).loc(), "Extraneous text after module 'end'");
    ok = false;
  }

  if (!ok) {
    return nullptr;
  }
  return std::make_unique<Module>(loc, name, std::vector<std::string>(),
				  std::move(imports), std::move(elems));
}

std::unique_ptr<Module> Parser::parseHeader() {
  Location loc = lexer.get(0).loc();

  if (!expect(Token::Kind::keywordHeader)) {
    error(lexer.get(0).loc(), "Expected 'header' declaration");
    return nullptr;
  }
  std::string name;
  if (!expect(Token::Kind::ident, name)) {
    error(lexer.get(0).loc(), "Expected module name");
    return nullptr;
  }

  std::vector<std::string> params;
  if (lexer.get(0).is(Token::Kind::puncBracketL)) {
    lexer.shift();
    while (true) {
      if (!expect(Token::Kind::puncDollar)) {
	error(lexer.get(0).loc(), "Expected type variable as header parameter");
	return nullptr;
      }
      std::string param;
      if (!expect(Token::Kind::ident, param)) {
	error(lexer.get(0).loc(), "Expected type variable name");
	return nullptr;
      }
      params.push_back(param);
      if (!lexer.get(0).is(Token::Kind::puncComma)) {
	break;
      }
      lexer.shift();
    }
    if (!expect(Token::Kind::puncBracketR)) {
      error(lexer.get(0).loc(), "Expected right bracket after header parameters");
      return nullptr;
    }
  }

  if (!expect(Token::Kind::keywordIs)) {
    error(lexer.get(0).loc(), "Expected 'is' after module name");
    return nullptr;
  }

  bool ok = true;

  std::vector<std::unique_ptr<Import>> imports;
  while (lexer.get(0).is(Token::Kind::keywordImport)) {
    std::unique_ptr<Import> import = parseImport();
    if (import) {
      imports.push_back(std::move(import));
    } else {
      ok = false;
    }
  }

  std::vector<std::unique_ptr<ModuleElem>> elems;
  if (!parseList(&Parser::parseHeaderElem, Token::Kind::keywordEnd, true, elems)) {
    ok = false;
  }

  if (!expect(Token::Kind::keywordEnd)) {
    error(lexer.get(0).loc(), "Missing 'end' in header");
    ok = false;
  }
  if (lexer.moreInput()) {
    error(lexer.get(0).loc(), "Extraneous text after header 'end'");
    ok = false;
  }

  if (!ok) {
    return nullptr;
  }
  return std::make_unique<Module>(loc, name, params, std::move(imports), std::move(elems));
}

std::unique_ptr<Import> Parser::parseImport() {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // 'import'
  std::string name;
  if (!expect(Token::Kind::ident, name)) {
    error(lexer.get(0).loc(), "Expected import name");
    return nullptr;
  }
  if (!expect(Token::Kind::puncSemicolon)) {
    error(lexer.get(0).loc(), "Expected semicolon");
    return nullptr;
  }
  return std::make_unique<Import>(loc, name);
}

std::unique_ptr<ModuleElem> Parser::parseModuleElem() {
  if (lexer.get(0).is(Token::Kind::keywordStruct)) {
    return parseStructDefn();
  } else if (lexer.get(0).is(Token::Kind::keywordVarstruct)) {
    return parseVarStructDefn();
  } else if (lexer.get(0).is(Token::Kind::keywordEnum)) {
    return parseEnumDefn();
  } else if (lexer.get(0).is(Token::Kind::keywordConst)) {
    return parseConstDefn();
  } else if (lexer.get(0).is(Token::Kind::keywordFunc)) {
    return parseFuncDefn(false);
  } else {
    error(lexer.get(0).loc(), "Expected constant, struct, or function definition");
    lexer.shift(); // avoid an infinite loop
    return nullptr;
  }
}

std::unique_ptr<ModuleElem> Parser::parseHeaderElem() {
  if (lexer.get(0).is(Token::Kind::keywordStruct)) {
    return parseStructDefn();
  } else if (lexer.get(0).is(Token::Kind::keywordVarstruct)) {
    return parseVarStructDefn();
  } else if (lexer.get(0).is(Token::Kind::keywordEnum)) {
    return parseEnumDefn();
  } else if (lexer.get(0).is(Token::Kind::keywordNativetype)) {
    return parseNativeTypeDefn();
  } else if (lexer.get(0).is(Token::Kind::keywordConst)) {
    return parseConstDefn();
  } else if (lexer.get(0).is(Token::Kind::keywordFunc) ||
	     lexer.get(0).is(Token::Kind::keywordNativefunc)) {
    return parseFuncDefn(true);
  } else {
    error(lexer.get(0).loc(), "Expected constant, struct, native type, or function definition");
    lexer.shift(); // avoid an infinite loop
    return nullptr;
  }
}

std::unique_ptr<StructDefn> Parser::parseStructDefn() {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // 'struct'
  std::string name;
  if (!expect(Token::Kind::ident, name)) {
    error(lexer.get(0).loc(), "Expected struct name");
    return nullptr;
  }
  if (!expect(Token::Kind::keywordIs)) {
    error(lexer.get(0).loc(), "Expected 'is' after struct name");
    return nullptr;
  }
  std::vector<std::unique_ptr<Field>> fields;
  if (!parseList<Field>(&Parser::parseField, Token::Kind::keywordEnd, false, fields)) {
    return nullptr;
  }
  if (!expect(Token::Kind::keywordEnd)) {
    error(lexer.get(0).loc(), "Expected 'end' after struct definition");
    return nullptr;
  }
  return std::make_unique<StructDefn>(loc, name, std::move(fields));
}

std::unique_ptr<VarStructDefn> Parser::parseVarStructDefn() {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // 'varstruct'
  std::string name;
  if (!expect(Token::Kind::ident, name)) {
    error(lexer.get(0).loc(), "Expected varstruct name");
    return nullptr;
  }
  if (!expect(Token::Kind::keywordIs)) {
    error(lexer.get(0).loc(), "Expected 'is' after varstruct name");
    return nullptr;
  }
  std::vector<std::unique_ptr<Field>> fields;
  while (!lexer.get(0).is(Token::Kind::keywordEnd) &&
	 !lexer.get(0).is(Token::Kind::keywordSubstruct)) {
    std::unique_ptr<Field> field = parseField();
    if (!field) {
      return nullptr;
    }
    fields.push_back(std::move(field));
  }
  std::vector<std::unique_ptr<SubStructDefn>> subStructs;
  do {
    std::unique_ptr<SubStructDefn> subStruct = parseSubStructDefn();
    if (!subStruct) {
      return nullptr;
    }
    subStructs.push_back(std::move(subStruct));
  } while (!lexer.get(0).is(Token::Kind::keywordEnd));
  if (!expect(Token::Kind::keywordEnd)) {
    error(lexer.get(0).loc(), "Expected 'end' after varstruct definition");
    return nullptr;
  }
  return std::make_unique<VarStructDefn>(loc, name, std::move(fields), std::move(subStructs));
}

std::unique_ptr<SubStructDefn> Parser::parseSubStructDefn() {
  Location loc = lexer.get(0).loc();
  if (!expect(Token::Kind::keywordSubstruct)) {
    error(lexer.get(0).loc(), "Expected 'substruct'");
    return nullptr;
  }
  std::string name;
  if (!expect(Token::Kind::ident, name)) {
    error(lexer.get(0).loc(), "Expected substruct name");
    return nullptr;
  }
  if (!expect(Token::Kind::keywordIs)) {
    error(lexer.get(0).loc(), "Expected 'is' after substruct name");
    return nullptr;
  }
  std::vector<std::unique_ptr<Field>> fields;
  if (!parseList<Field>(&Parser::parseField, Token::Kind::keywordEnd, true, fields)) {
    return nullptr;
  }
  if (!expect(Token::Kind::keywordEnd)) {
    error(lexer.get(0).loc(), "Expected 'end' after substruct definition");
    return nullptr;
  }
  return std::make_unique<SubStructDefn>(loc, name, std::move(fields));
}

std::unique_ptr<Field> Parser::parseField() {
  Location loc = lexer.get(0).loc();
  std::string name;
  if (!expect(Token::Kind::ident, name)) {
    error(lexer.get(0).loc(), "Expected field name");
    return nullptr;
  }
  if (!expect(Token::Kind::puncColon)) {
    error(lexer.get(0).loc(), "Expected ':' after field name");
    return nullptr;
  }
  std::unique_ptr<TypeRef> type = parseTypeRef();
  if (!type) {
    return nullptr;
  }
  if (!expect(Token::Kind::puncSemicolon)) {
    error(lexer.get(0).loc(), "Expected ';' after field");
    return nullptr;
  }
  return std::make_unique<Field>(loc, name, std::move(type));
}

std::unique_ptr<EnumDefn> Parser::parseEnumDefn() {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // 'enum'
  std::string name;
  if (!expect(Token::Kind::ident, name)) {
    error(lexer.get(0).loc(), "Expected enum name");
    return nullptr;
  }
  if (!expect(Token::Kind::keywordIs)) {
    error(lexer.get(0).loc(), "Expected 'is' after enum name");
    return nullptr;
  }
  std::vector<std::string> members;
  while (lexer.get(0).is(Token::Kind::ident)) {
    members.push_back(lexer.get(0).str());
    lexer.shift();
    if (!expect(Token::Kind::puncSemicolon)) {
      error(lexer.get(0).loc(), "Expected ';' after enum member");
      return nullptr;
    }
  }
  if (!expect(Token::Kind::keywordEnd)) {
    error(lexer.get(0).loc(), "Expected 'end' after enum definition");
    return nullptr;
  }
  return std::make_unique<EnumDefn>(loc, name, std::move(members));
}

std::unique_ptr<NativeTypeDefn> Parser::parseNativeTypeDefn() {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // 'nativetype'
  std::vector<std::string> attrs;
  if (lexer.get(0).is(Token::Kind::stringLiteral)) {
    while (true) {
      std::string attr;
      if (!expect(Token::Kind::stringLiteral, attr)) {
	error(lexer.get(0).loc(), "Expected string literal as nativetype attribute");
	return nullptr;
      }
      attrs.push_back(attr);
      if (!lexer.get(0).is(Token::Kind::puncComma)) {
	break;
      }
      lexer.shift();
    }
  }
  std::string name;
  if (!expect(Token::Kind::ident, name)) {
    error(lexer.get(0).loc(), "Expected native type name");
    return nullptr;
  }
  if (!expect(Token::Kind::puncSemicolon)) {
    error(lexer.get(0).loc(), "Expected ';' after native type definition");
    return nullptr;
  }
  return std::make_unique<NativeTypeDefn>(loc, name, attrs);
}

std::unique_ptr<ConstDefn> Parser::parseConstDefn() {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // 'const'
  std::string name;
  if (!expect(Token::Kind::ident, name)) {
    error(lexer.get(0).loc(), "Expected constant name");
    return nullptr;
  }
  if (!expect(Token::Kind::puncEq)) {
    error(lexer.get(0).loc(), "Expected '=' after constant name");
    return nullptr;
  }
  std::unique_ptr<Expr> val = parseExpr();
  if (!val) {
    return nullptr;
  }
  if (!expect(Token::Kind::puncSemicolon)) {
    error(lexer.get(0).loc(), "Expected ';' after constant");
    return nullptr;
  }
  return std::make_unique<ConstDefn>(loc, name, std::move(val));
}

std::unique_ptr<FuncDefn> Parser::parseFuncDefn(bool isDecl) {
  Location loc = lexer.get(0).loc();
  bool native = lexer.get(0).is(Token::Kind::keywordNativefunc);
  lexer.shift(); // 'func' or 'nativefunc'
  std::string name;
  if (!expect(Token::Kind::ident, name)) {
    error(lexer.get(0).loc(), "Expected function name");
    return nullptr;
  }
  if (!expect(Token::Kind::puncParenL)) {
    error(lexer.get(0).loc(), "Expected left paren after function name");
    return nullptr;
  }
  bool ok = true;
  std::vector<std::unique_ptr<Arg>> args;
  if (!parseSepList<Arg>(&Parser::parseArg, Token::Kind::puncComma,
			 Token::Kind::puncParenR, true, args)) {
    ok = false;
  }
  if (!expect(Token::Kind::puncParenR)) {
    error(lexer.get(0).loc(), "Expected right paren");
    ok = false;
  }
  std::unique_ptr<TypeRef> returnType;
  if (lexer.get(0).is(Token::Kind::puncArrowR)) {
    lexer.shift();
    if (!(returnType = parseTypeRef())) {
      ok = false;
    }
  }

  std::unique_ptr<Block> block;
  if (isDecl) {
    if (!expect(Token::Kind::puncSemicolon)) {
      error(lexer.get(0).loc(), "Expected semicolon after function declaration");
      ok = false;
    }

  } else {
    if (!expect(Token::Kind::keywordIs)) {
      error(lexer.get(0).loc(), "Expected 'is' after function signature");
      ok = false;
    }
    block = parseBlock(Token::Kind::keywordEnd);
    if (!block) {
      ok = false;
    }
    if (!expect(Token::Kind::keywordEnd)) {
      error(lexer.get(0).loc(), "Expected 'end' after function definition");
      ok = false;
    }
  }

  if (!ok) {
    return nullptr;
  }
  return std::make_unique<FuncDefn>(loc, native, name, std::move(args),
				    std::move(returnType), std::move(block));
}

std::unique_ptr<Arg> Parser::parseArg() {
  Location loc = lexer.get(0).loc();
  std::string name;
  if (!expect(Token::Kind::ident, name)) {
    error(lexer.get(0).loc(), "Expected argument");
    return nullptr;
  }
  if (!expect(Token::Kind::puncColon)) {
    error(lexer.get(0).loc(), "Expected ':' after argument name");
    return nullptr;
  }
  std::unique_ptr<TypeRef> type = parseTypeRef();
  if (!type) {
    return nullptr;
  }
  return std::make_unique<Arg>(loc, name, std::move(type));
}

std::unique_ptr<TypeRef> Parser::parseTypeRef() {
  Location loc = lexer.get(0).loc();

  if (lexer.get(0).is(Token::Kind::puncDollar)) {
    lexer.shift();
    std::string name;
    if (!expect(Token::Kind::ident, name)) {
      error(lexer.get(0).loc(), "Expected type name");
      return nullptr;
    }
    return std::make_unique<TypeVarRef>(loc, name);

  } else {
    std::string name;
    if (!expect(Token::Kind::ident, name)) {
      error(lexer.get(0).loc(), "Expected type name");
      return nullptr;
    }

    if (lexer.get(0).is(Token::Kind::puncBracketL)) {
      lexer.shift();
      bool hasReturnType = false;
      std::vector<std::unique_ptr<TypeRef>> params;
      if (!lexer.get(0).is(Token::Kind::puncBracketR) &&
	  !lexer.get(0).is(Token::Kind::puncArrowR)) {
	while (true) {
	  std::unique_ptr<TypeRef> param = parseTypeRef();
	  if (!param) {
	    return nullptr;
	  }
	  params.push_back(std::move(param));
	  if (!lexer.get(0).is(Token::Kind::puncComma)) {
	    break;
	  }
	  lexer.shift();
	}
      }
      if (lexer.get(0).is(Token::Kind::puncArrowR)) {
	lexer.shift();
	hasReturnType = true;
	std::unique_ptr<TypeRef> param = parseTypeRef();
	if (!param) {
	  return nullptr;
	}
	params.push_back(std::move(param));
      }
      if (!expect(Token::Kind::puncBracketR)) {
	error(lexer.get(0).loc(), "Expected right bracket after type parameters");
	return nullptr;
      }
      return std::make_unique<ParamTypeRef>(loc, name, hasReturnType, std::move(params));

    } else {
      return std::make_unique<SimpleTypeRef>(loc, name);
    }
  }
}

std::unique_ptr<Block> Parser::parseBlock(Token::Kind end1, Token::Kind end2, Token::Kind end3) {
  Location loc = lexer.get(0).loc();
  std::vector<std::unique_ptr<Stmt>> stmts;
  bool ok = true;
  while (lexer.get(0).kind() != Token::Kind::eof) {
    Token tok = lexer.get(0);
    if (tok.is(end1) ||
	(end2 != Token::Kind::error && tok.is(end2)) ||
	(end3 != Token::Kind::error && tok.is(end3))) {
      break;
    }
    std::unique_ptr<Stmt> stmt = parseStmt();
    if (stmt) {
      stmts.push_back(std::move(stmt));
    } else {
      ok = false;
    }
  }
  if (!ok) {
    return nullptr;
  }
  return std::make_unique<Block>(loc, std::move(stmts));
}

std::unique_ptr<Stmt> Parser::parseStmt() {
  Token tok = lexer.get(0);
  if (tok.is(Token::Kind::keywordVar)) {
    return parseVarStmt();
  } else if (tok.is(Token::Kind::keywordIf)) {
    return parseIfStmt();
  } else if (tok.is(Token::Kind::keywordWhile)) {
    return parseWhileStmt();
  } else if (tok.is(Token::Kind::keywordFor)) {
    return parseForStmt();
  } else if (tok.is(Token::Kind::keywordBreak)) {
    return parseBreakStmt();
  } else if (tok.is(Token::Kind::keywordContinue)) {
    return parseContinueStmt();
  } else if (tok.is(Token::Kind::keywordTypematch)) {
    return parseTypematchStmt();
  } else if (tok.is(Token::Kind::keywordReturn)) {
    return parseReturnStmt();
  } else {
    return parseExprOrAssignStmt();
  }
}

std::unique_ptr<VarStmt> Parser::parseVarStmt() {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // 'var'
  std::string name;
  if (!expect(Token::Kind::ident, name)) {
    error(lexer.get(0).loc(), "Expected local variable name");
    return nullptr;
  }
  if (!expect(Token::Kind::puncEq)) {
    error(lexer.get(0).loc(), "Expected local variable initializer");
    return nullptr;
  }
  std::unique_ptr<Expr> expr = parseExpr();
  if (!expr) {
    return nullptr;
  }
  if (!expect(Token::Kind::puncSemicolon)) {
    error(lexer.get(0).loc(), "Expected semicolon after local variable definition");
    return nullptr;
  }
  return std::make_unique<VarStmt>(loc, name, std::move(expr));
}

std::unique_ptr<IfStmt> Parser::parseIfStmt() {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // 'if'
  std::vector<std::unique_ptr<Expr>> tests;
  std::vector<std::unique_ptr<Block>> blocks;
  while (true) {
    std::unique_ptr<Expr> test = parseExpr();
    if (!test) {
      return nullptr;
    }
    if (!expect(Token::Kind::keywordThen)) {
      error(lexer.get(0).loc(), "Expected 'then'");
      return nullptr;
    }
    std::unique_ptr<Block> block = parseBlock(Token::Kind::keywordElseif,
					      Token::Kind::keywordElse,
					      Token::Kind::keywordEnd);
    if (!block) {
      return nullptr;
    }
    tests.push_back(std::move(test));
    blocks.push_back(std::move(block));
    if (!lexer.get(0).is(Token::Kind::keywordElseif)) {
      break;
    }
    lexer.shift();
  }
  std::unique_ptr<Block> elseBlock;
  if (lexer.get(0).is(Token::Kind::keywordElse)) {
    lexer.shift();
    elseBlock = parseBlock(Token::Kind::keywordEnd);
    if (!elseBlock) {
      return nullptr;
    }
  }
  if (!expect(Token::Kind::keywordEnd)) {
    error(lexer.get(0).loc(), "Expected 'end' after 'if' statement");
    return nullptr;
  }
  return std::make_unique<IfStmt>(loc, std::move(tests), std::move(blocks), std::move(elseBlock));
}

std::unique_ptr<WhileStmt> Parser::parseWhileStmt() {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // 'while'
  std::unique_ptr<Expr> test = parseExpr();
  if (!test) {
    return nullptr;
  }
  if (!expect(Token::Kind::keywordDo)) {
    error(lexer.get(0).loc(), "Expected 'do'");
    return nullptr;
  }
  std::unique_ptr<Block> block = parseBlock(Token::Kind::keywordEnd);
  if (!block) {
    return nullptr;
  }
  if (!expect(Token::Kind::keywordEnd)) {
    error(lexer.get(0).loc(), "Expected 'end' after 'while' statement");
    return nullptr;
  }
  return std::make_unique<WhileStmt>(loc, std::move(test), std::move(block));
}

std::unique_ptr<ForStmt> Parser::parseForStmt() {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // 'for'
  std::string var;
  if (!expect(Token::Kind::ident, var)) {
    error(lexer.get(0).loc(), "Expected variable name in 'for' statement");
    return nullptr;
  }
  if (!expect(Token::Kind::puncColon)) {
    error(lexer.get(0).loc(), "Expected colon after variable name in 'for' statement");
    return nullptr;
  }
  std::unique_ptr<Expr> expr1 = parseExpr();
  if (!expr1) {
    return nullptr;
  }
  std::unique_ptr<Expr> expr2;
  if (lexer.get(0).is(Token::Kind::puncPeriodPeriod)) {
    lexer.shift();
    expr2 = parseExpr();
    if (!expr2) {
      return nullptr;
    }
  }
  if (!expect(Token::Kind::keywordDo)) {
    error(lexer.get(0).loc(), "Expected 'do'");
    return nullptr;
  }
  std::unique_ptr<Block> block = parseBlock(Token::Kind::keywordEnd);
  if (!block) {
    return nullptr;
  }
  if (!expect(Token::Kind::keywordEnd)) {
    error(lexer.get(0).loc(), "Expected 'end' after 'for' statement");
    return nullptr;
  }
  return std::make_unique<ForStmt>(loc, var, std::move(expr1), std::move(expr2), std::move(block));
}

std::unique_ptr<BreakStmt> Parser::parseBreakStmt() {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // 'break'
  if (!expect(Token::Kind::puncSemicolon)) {
    error(lexer.get(0).loc(), "Expected semicolon after 'break' statement");
    return nullptr;
  }
  return std::make_unique<BreakStmt>(loc);
}

std::unique_ptr<ContinueStmt> Parser::parseContinueStmt() {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // 'continue'
  if (!expect(Token::Kind::puncSemicolon)) {
    error(lexer.get(0).loc(), "Expected semicolon after 'continue' statement");
    return nullptr;
  }
  return std::make_unique<ContinueStmt>(loc);
}

std::unique_ptr<TypematchStmt> Parser::parseTypematchStmt() {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // 'typematch'
  std::unique_ptr<Expr> expr = parseExpr();
  if (!expr) {
    return nullptr;
  }
  if (!expect(Token::Kind::keywordIs)) {
    error(lexer.get(0).loc(), "Expected 'is'");
    return nullptr;
  }
  std::vector<std::unique_ptr<TypematchCase>> cases;
  if (!parseList(&Parser::parseTypematchCase, Token::Kind::keywordEnd, false, cases)) {
    return nullptr;
  }
  if (!expect(Token::Kind::keywordEnd)) {
    error(lexer.get(0).loc(), "Expected 'end' after 'typematch' statement");
    return nullptr;
  }
  return std::make_unique<TypematchStmt>(loc, std::move(expr), std::move(cases));
}

std::unique_ptr<TypematchCase> Parser::parseTypematchCase() {
  Location loc = lexer.get(0).loc();
  std::string var;
  std::unique_ptr<TypeRef> type;
  if (lexer.get(0).is(Token::Kind::keywordCase)) {
    lexer.shift();
    if (!expect(Token::Kind::ident, var)) {
      error(lexer.get(0).loc(), "Expected variable name");
      return nullptr;
    }
    if (!expect(Token::Kind::puncColon)) {
      error(lexer.get(0).loc(), "Expected ':'");
      return nullptr;
    }
    type = parseTypeRef();
    if (!type) {
      return nullptr;
    }
    if (!expect(Token::Kind::puncColon)) {
      error(lexer.get(0).loc(), "Expected ':'");
      return nullptr;
    }
  } else if (lexer.get(0).is(Token::Kind::keywordDefault)) {
    lexer.shift();
    if (!expect(Token::Kind::puncColon)) {
      error(lexer.get(0).loc(), "Expected ':'");
      return nullptr;
    }
  }
  std::unique_ptr<Block> block = parseBlock(Token::Kind::keywordCase,
					    Token::Kind::keywordDefault,
					    Token::Kind::keywordEnd);
  if (!block) {
    return nullptr;
  }
  return std::make_unique<TypematchCase>(loc, var, std::move(type), std::move(block));
}

std::unique_ptr<ReturnStmt> Parser::parseReturnStmt() {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // 'return'
  std::unique_ptr<Expr> expr;
  if (!lexer.get(0).is(Token::Kind::puncSemicolon)) {
    expr = parseExpr();
    if (!expr) {
      return nullptr;
    }
  }
  if (!expect(Token::Kind::puncSemicolon)) {
    error(lexer.get(0).loc(), "Expected semicolon after return statement");
    return nullptr;
  }
  return std::make_unique<ReturnStmt>(loc, std::move(expr));
}

std::unique_ptr<Stmt> Parser::parseExprOrAssignStmt() {
  Location loc = lexer.get(0).loc();
  std::unique_ptr<Expr> expr = parseExpr();
  if (!expr) {
    return nullptr;
  }
  std::unique_ptr<Expr> rhs;
  if (lexer.get(0).is(Token::Kind::puncEq)) {
    lexer.shift();
    rhs = parseExpr();
    if (!rhs) {
      return nullptr;
    }
  }
  if (!expect(Token::Kind::puncSemicolon)) {
    error(lexer.get(0).loc(), "Expected semicolon after expression");
    return nullptr;
  }
  if (rhs) {
    return std::make_unique<AssignStmt>(loc, std::move(expr), std::move(rhs));
  } else {
    return std::make_unique<ExprStmt>(loc, std::move(expr));
  }
}

std::unique_ptr<Expr> Parser::parseExpr() {
  return parseCondOrExpr();
}

std::unique_ptr<Expr> Parser::parseCondOrExpr() {
  static std::vector<ParseBinaryOpInfo> opInfoVec {
    { Token::Kind::puncBarBar, BinaryOp::condOr },
  };
  return parseLeftBinary(&Parser::parseCondAndExpr, opInfoVec);
}

std::unique_ptr<Expr> Parser::parseCondAndExpr() {
  static std::vector<ParseBinaryOpInfo> opInfoVec {
    { Token::Kind::puncAmpersandAmpersand, BinaryOp::condAnd }
  };
  return parseLeftBinary(&Parser::parseOrExpr, opInfoVec);
}

std::unique_ptr<Expr> Parser::parseOrExpr() {
  static std::vector<ParseBinaryOpInfo> opInfoVec {
    { Token::Kind::puncBar,   BinaryOp::orOp },
    { Token::Kind::puncCaret, BinaryOp::xorOp }
  };
  return parseLeftBinary(&Parser::parseAndExpr, opInfoVec);
}

std::unique_ptr<Expr> Parser::parseAndExpr() {
  static std::vector<ParseBinaryOpInfo> opInfoVec {
    { Token::Kind::puncAmpersand, BinaryOp::andOp }
  };
  return parseLeftBinary(&Parser::parseEqualExpr, opInfoVec);
}

std::unique_ptr<Expr> Parser::parseEqualExpr() {
  static std::vector<ParseBinaryOpInfo> opInfoVec {
    { Token::Kind::puncEqEq,       BinaryOp::eq },
    { Token::Kind::puncExclamEq,   BinaryOp::ne },
    { Token::Kind::puncEqEqEq,     BinaryOp::same },
    { Token::Kind::puncExclamEqEq, BinaryOp::notSame }
  };
  return parseBinary(&Parser::parseCmpExpr, opInfoVec);
}

std::unique_ptr<Expr> Parser::parseCmpExpr() {
  static std::vector<ParseBinaryOpInfo> opInfoVec {
    { Token::Kind::puncLt,   BinaryOp::lt },
    { Token::Kind::puncGt,   BinaryOp::gt },
    { Token::Kind::puncLtEq, BinaryOp::le },
    { Token::Kind::puncGtEq, BinaryOp::ge }
  };
  return parseBinary(&Parser::parseShiftExpr, opInfoVec);
}

std::unique_ptr<Expr> Parser::parseShiftExpr() {
  static std::vector<ParseBinaryOpInfo> opInfoVec {
    { Token::Kind::puncLtLt, BinaryOp::shl },
    { Token::Kind::puncGtGt, BinaryOp::shr }
  };
  return parseLeftBinary(&Parser::parseAddExpr, opInfoVec);
}

std::unique_ptr<Expr> Parser::parseAddExpr() {
  static std::vector<ParseBinaryOpInfo> opInfoVec {
    { Token::Kind::puncPlus,  BinaryOp::add },
    { Token::Kind::puncMinus, BinaryOp::sub }
  };
  return parseLeftBinary(&Parser::parseMulExpr, opInfoVec);
}

std::unique_ptr<Expr> Parser::parseMulExpr() {
  static std::vector<ParseBinaryOpInfo> opInfoVec {
    { Token::Kind::puncAsterisk, BinaryOp::mul },
    { Token::Kind::puncSlash,    BinaryOp::div },
    { Token::Kind::puncPercent,  BinaryOp::mod }
  };
  return parseLeftBinary(&Parser::parseUnOpExpr, opInfoVec);
}

std::unique_ptr<Expr> Parser::parseUnOpExpr() {
  static std::vector<ParseUnaryOpInfo> opInfoVec {
    { Token::Kind::puncMinus, UnaryOp::neg },
    { Token::Kind::puncExclam, UnaryOp::notOp },
    { Token::Kind::puncSharp, UnaryOp::length },
    { Token::Kind::keywordVarstruct, UnaryOp::varstruct },
    { Token::Kind::keywordSubstruct, UnaryOp::substruct }
  };
  return parseLeftUnary(&Parser::parseSuffixExpr, opInfoVec);
}

std::unique_ptr<Expr> Parser::parseSuffixExpr() {
  std::unique_ptr<Expr> expr = parsePostfixExpr();
  if (!expr) {
    return nullptr;
  }
  Token tok = lexer.get(0);
  if (tok.is(Token::Kind::puncQuestion)) {
    lexer.shift();
    expr = std::make_unique<PropagateExpr>(tok.loc(), std::move(expr));
  } else if (tok.is(Token::Kind::puncExclam)) {
    lexer.shift();
    expr = std::make_unique<CheckExpr>(tok.loc(), std::move(expr));
  }
  return expr;
}

std::unique_ptr<Expr> Parser::parsePostfixExpr() {
  std::unique_ptr<Expr> expr = parseFactorExpr();
  if (!expr) {
    return nullptr;
  }
  while (true) {
    Token tok = lexer.get(0);
    if (tok.is(Token::Kind::puncParenL)) {
      expr = parseCallExpr(std::move(expr));
    } else if (tok.is(Token::Kind::puncPeriod)) {
      expr = parseMemberExpr(std::move(expr));
    } else if (tok.is(Token::Kind::puncBracketL)) {
      expr = parseIndexExpr(std::move(expr));
    } else {
      break;
    }
    if (!expr) {
      return nullptr;
    }
  }
  return expr;
}

std::unique_ptr<Expr> Parser::parseCallExpr(std::unique_ptr<Expr> func) {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // left paren
  std::vector<std::unique_ptr<Expr>> args;
  if (!parseSepList<Expr>(&Parser::parseExpr, Token::Kind::puncComma, Token::Kind::puncParenR,
			  true, args)) {
    return nullptr;
  }
  if (!expect(Token::Kind::puncParenR)) {
    error(lexer.get(0).loc(), "Expected right paren after function call");
    return nullptr;
  }
  return std::make_unique<CallExpr>(loc, std::move(func), std::move(args));
}

std::unique_ptr<Expr> Parser::parseMemberExpr(std::unique_ptr<Expr> lhs) {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // period
  std::string member;
  if (!expect(Token::Kind::ident, member)) {
    error(lexer.get(0).loc(), "Expected member name after period");
    return nullptr;
  }
  return std::make_unique<MemberExpr>(loc, std::move(lhs), member);
}

std::unique_ptr<Expr> Parser::parseIndexExpr(std::unique_ptr<Expr> obj) {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // left bracket
  std::unique_ptr<Expr> idx = parseExpr();
  if (!idx) {
    return nullptr;
  }
  if (!expect(Token::Kind::puncBracketR)) {
    error(lexer.get(0).loc(), "Expected right bracket after index");
    return nullptr;
  }
  return std::make_unique<IndexExpr>(loc, std::move(obj), std::move(idx));
}

std::unique_ptr<Expr> Parser::parseFactorExpr() {
  Token tok = lexer.get(0);
  if (tok.is(Token::Kind::puncParenL)) {
    return parseParenExpr();
  } else if (tok.is(Token::Kind::ident)) {
    lexer.shift();
    return std::make_unique<IdentExpr>(tok.loc(), tok.str());
  } else if (tok.is(Token::Kind::keywordNew)) {
    return parseNewExpr();
  } else if (tok.is(Token::Kind::keywordMake)) {
    return parseMakeExpr();
  } else if (tok.is(Token::Kind::puncAmpersand)) {
    return parseFuncPointerExpr();
  } else if (tok.is(Token::Kind::keywordNil)) {
    return parseNilExpr();
  } else if (tok.is(Token::Kind::keywordError)) {
    return parseErrorExpr();
  } else if (tok.is(Token::Kind::keywordValid)) {
    return parseValidExpr();
  } else if (tok.is(Token::Kind::keywordOk)) {
    return parseOkExpr();
  } else if (tok.is(Token::Kind::puncBracketL)) {
    return parseLitVectorExpr();
  } else if (tok.is(Token::Kind::puncBraceL)) {
    return parseLitSetOrMapExpr();
  } else if (tok.is(Token::Kind::decimalIntLiteral)) {
    lexer.shift();
    return std::make_unique<LitIntExpr>(tok.loc(), tok.str(), 10);
  } else if (tok.is(Token::Kind::binaryIntLiteral)) {
    lexer.shift();
    return std::make_unique<LitIntExpr>(tok.loc(), tok.str(), 2);
  } else if (tok.is(Token::Kind::octalIntLiteral)) {
    lexer.shift();
    return std::make_unique<LitIntExpr>(tok.loc(), tok.str(), 8);
  } else if (tok.is(Token::Kind::hexIntLiteral)) {
    lexer.shift();
    return std::make_unique<LitIntExpr>(tok.loc(), tok.str(), 16);
  } else if (tok.is(Token::Kind::floatLiteral)) {
    lexer.shift();
    return std::make_unique<LitFloatExpr>(tok.loc(), tok.str());
  } else if (tok.is(Token::Kind::keywordTrue)) {
    lexer.shift();
    return std::make_unique<LitBoolExpr>(tok.loc(), true);
  } else if (tok.is(Token::Kind::keywordFalse)) {
    lexer.shift();
    return std::make_unique<LitBoolExpr>(tok.loc(), false);
  } else if (tok.is(Token::Kind::charLiteral)) {
    lexer.shift();
    return std::make_unique<LitCharExpr>(tok.loc(), tok.str());
  } else if (tok.is(Token::Kind::stringLiteral)) {
    lexer.shift();
    return std::make_unique<LitStringExpr>(tok.loc(), tok.str());
  } else if (tok.is(Token::Kind::interpString)) {
    return parseInterpStringExpr();
  } else {
    error(tok.loc(), "Unexpected token");
    lexer.shift(); // avoid an infinite loop
    return nullptr;
  }
}

std::unique_ptr<Expr> Parser::parseParenExpr() {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // left paren
  std::unique_ptr<Expr> expr = parseExpr();
  if (!expr) {
    return nullptr;
  }
  if (!expect(Token::Kind::puncParenR)) {
    error(lexer.get(0).loc(), "Missing right paren in expression");
    return nullptr;
  }
  return std::make_unique<ParenExpr>(loc, std::move(expr));
}

std::unique_ptr<Expr> Parser::parseNewExpr() {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // 'new'
  std::unique_ptr<TypeRef> type = parseTypeRef();
  if (!type) {
    return nullptr;
  }
  return std::make_unique<NewExpr>(loc, std::move(type));
}

std::unique_ptr<Expr> Parser::parseMakeExpr() {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // 'make'
  std::unique_ptr<TypeRef> type = parseTypeRef();
  if (!type) {
    return nullptr;
  }
  if (!expect(Token::Kind::puncParenL)) {
    error(lexer.get(0).loc(), "Expected left paren after 'make' type");
    return nullptr;
  }
  std::vector<std::unique_ptr<FieldInit>> fieldInits;
  if (!parseSepList<FieldInit>(&Parser::parseFieldInit, Token::Kind::puncComma,
			       Token::Kind::puncParenR, true, fieldInits)) {
    return nullptr;
  }
  if (!expect(Token::Kind::puncParenR)) {
    error(lexer.get(0).loc(), "Expected right paren after 'make' field inits");
    return nullptr;
  }
  return std::make_unique<MakeExpr>(loc, std::move(type), std::move(fieldInits));
}

std::unique_ptr<FieldInit> Parser::parseFieldInit() {
  Location loc = lexer.get(0).loc();
  std::string name;
  if (!expect(Token::Kind::ident, name)) {
    error(lexer.get(0).loc(), "Expected field name");
    return nullptr;
  }
  if (!expect(Token::Kind::puncColon)) {
    error(lexer.get(0).loc(), "Expected colon after field name");
    return nullptr;
  }
  std::unique_ptr<Expr> val = parseExpr();
  if (!val) {
    return nullptr;
  }
  return std::make_unique<FieldInit>(loc, name, std::move(val));
}

std::unique_ptr<Expr> Parser::parseFuncPointerExpr() {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // '&'
  std::string name;
  if (!expect(Token::Kind::ident, name)) {
    error(lexer.get(0).loc(), "Expected function name for function pointer");
    return nullptr;
  }
  if (!expect(Token::Kind::puncParenL)) {
    error(lexer.get(0).loc(), "Expected left paren after function name in function pointer");
    return nullptr;
  }
  std::vector<std::unique_ptr<TypeRef>> argTypes;
  if (!parseSepList<TypeRef>(&Parser::parseTypeRef, Token::Kind::puncComma, Token::Kind::puncParenR,
			     true, argTypes)) {
    return nullptr;
  }
  if (!expect(Token::Kind::puncParenR)) {
    error(lexer.get(0).loc(), "Expected right paren after function pointer argument types");
    return nullptr;
  }
  return std::make_unique<FuncPointerExpr>(loc, name, std::move(argTypes));
}

std::unique_ptr<Expr> Parser::parseNilExpr() {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // 'nil'
  if (lexer.get(0).kind() == Token::Kind::puncBracketL) {
    lexer.shift();
    std::unique_ptr<TypeRef> type = parseTypeRef();
    if (!type) {
      return nullptr;
    }
    if (!expect(Token::Kind::puncBracketR)) {
      error(lexer.get(0).loc(), "Expected right bracket after 'nil' type");
      return nullptr;
    }
    return std::make_unique<NilExpr>(loc, std::move(type));
  } else if (lexer.get(0).kind() == Token::Kind::puncParenL) {
    lexer.shift();
    std::unique_ptr<Expr> expr = parseExpr();
    if (!expr) {
      return nullptr;
    }
    if (!expect(Token::Kind::puncParenR)) {
      error(lexer.get(0).loc(), "Expected right paren after 'nil' expression");
      return nullptr;
    }
    return std::make_unique<NilTestExpr>(loc, std::move(expr));
  } else {
    error(lexer.get(0).loc(), "Expected left bracket or paren after 'nil'");
    return nullptr;
  }
}

std::unique_ptr<Expr> Parser::parseErrorExpr() {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // 'error'
  if (!expect(Token::Kind::puncBracketL)) {
    error(lexer.get(0).loc(), "Expected left bracket after 'error'");
    return nullptr;
  }
  std::unique_ptr<TypeRef> type;
  if (lexer.get(0).kind() != Token::Kind::puncBracketR) {
    type = parseTypeRef();
    if (!type) {
      return nullptr;
    }
  }
  if (!expect(Token::Kind::puncBracketR)) {
    error(lexer.get(0).loc(), "Expected right bracket after 'error' type");
    return nullptr;
  }
  return std::make_unique<ErrorExpr>(loc, std::move(type));
}

std::unique_ptr<Expr> Parser::parseValidExpr() {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // 'valid'
  if (!expect(Token::Kind::puncParenL)) {
    error(lexer.get(0).loc(), "Expected left paren after 'valid'");
    return nullptr;
  }
  std::unique_ptr<Expr> expr;
  if (lexer.get(0).kind() != Token::Kind::puncParenR) {
    expr = parseExpr();
    if (!expr) {
      return nullptr;
    }
  }
  if (!expect(Token::Kind::puncParenR)) {
    error(lexer.get(0).loc(), "Expected right paren after 'valid' arg");
    return nullptr;
  }
  return std::make_unique<ValidExpr>(loc, std::move(expr));
}

std::unique_ptr<Expr> Parser::parseOkExpr() {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // 'ok'
  if (!expect(Token::Kind::puncParenL)) {
    error(lexer.get(0).loc(), "Expected left paren after 'ok'");
    return nullptr;
  }
  std::unique_ptr<Expr> expr = parseExpr();
  if (!expr) {
    return nullptr;
  }
  if (!expect(Token::Kind::puncParenR)) {
    error(lexer.get(0).loc(), "Expected right paren after 'ok' arg");
    return nullptr;
  }
  return std::make_unique<OkExpr>(loc, std::move(expr));
}

std::unique_ptr<Expr> Parser::parseLitVectorExpr() {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // '['
  std::vector<std::unique_ptr<Expr>> vals;
  if (!parseSepList<Expr>(&Parser::parseExpr, Token::Kind::puncComma, Token::Kind::puncBracketR,
			  false, vals)) {
    return nullptr;
  }
  if (!expect(Token::Kind::puncBracketR)) {
    error(lexer.get(0).loc(), "Expected right bracket after Vector literal");
    return nullptr;
  }
  return std::make_unique<LitVectorExpr>(loc, std::move(vals));
}

std::unique_ptr<Expr> Parser::parseLitSetOrMapExpr() {
  Location loc = lexer.get(0).loc();
  lexer.shift(); // '{'
  std::unique_ptr<Expr> expr = parseExpr();
  if (!expr) {
    return nullptr;
  }
  if (lexer.get(0).is(Token::Kind::puncColon)) {
    std::vector<std::pair<std::unique_ptr<Expr>, std::unique_ptr<Expr>>> pairs;
    lexer.shift();
    std::unique_ptr<Expr> val = parseExpr();
    if (!val) {
      return nullptr;
    }
    pairs.push_back(std::make_pair(std::move(expr), std::move(val)));
    while (lexer.get(0).is(Token::Kind::puncComma)) {
      lexer.shift();
      expr = parseExpr();
      if (!expr) {
	return nullptr;
      }
      if (!expect(Token::Kind::puncColon)) {
	error(lexer.get(0).loc(), "Expected colon in Map literal");
	return nullptr;
      }
      val = parseExpr();
      if (!val) {
	return nullptr;
      }
      pairs.push_back(std::make_pair(std::move(expr), std::move(val)));
    }
    if (!expect(Token::Kind::puncBraceR)) {
      error(lexer.get(0).loc(), "Expected right brace after Map literal");
      return nullptr;
    }
    return std::make_unique<LitMapExpr>(loc, std::move(pairs));
  } else {
    std::vector<std::unique_ptr<Expr>> vals;
    vals.push_back(std::move(expr));
    while (lexer.get(0).is(Token::Kind::puncComma)) {
      lexer.shift();
      expr = parseExpr();
      if (!expr) {
	return nullptr;
      }
      vals.push_back(std::move(expr));
    }
    if (!expect(Token::Kind::puncBraceR)) {
      error(lexer.get(0).loc(), "Expected right brace after Set literal");
      return nullptr;
    }
    return std::make_unique<LitSetExpr>(loc, std::move(vals));
  }
}

std::unique_ptr<Expr> Parser::parseInterpStringExpr() {
  Location loc = lexer.get(0).loc();
  std::string s = lexer.get(0).str();
  lexer.shift();
  std::vector<std::unique_ptr<InterpStringPart>> parts;
  size_t i0 = 0;
  while (i0 < s.size()) {
    if (s[i0] == '{') {
      // expr:-WWW.PPPF
      // the expr cannot contain "'\{}:
      std::unique_ptr<Expr> expr;
      int width = 0;
      int precision = -1;
      char format = '\0';
      ++i0;
      size_t i1 = i0;
      while (i1 < s.size() &&
	     s[i1] != '"' && s[i1] != '\'' && s[i1] != '\\' &&
	     s[i1] != '{' && s[i1] != '}' && s[i1] != ':') {
	++i1;
      }
      std::string exprStr = s.substr(i0, i1 - i0);
      std::string exprPath = loc.hasPath() ? loc.path() + ":" + std::to_string(loc.line()) : "";
      Parser exprParser(exprStr, exprPath);
      expr = exprParser.parseExpr();
      if (!expr) {
	return nullptr;
      }
      if (exprParser.lexer.moreInput()) {
	error(loc, "Invalid argument expression in interpolated string");
	return nullptr;
      }
      i0 = i1;
      if (i0 < s.size() && s[i0] == ':') {
	++i0;
	if (i0 < s.size() && (s[i0] == '-' || (s[i0] >= '0' && s[i0] <= '9'))) {
	  size_t i1 = (s[i0] == '-') ? i0 + 1 : i0;
	  size_t i2;
	  for (i2 = i1; i2 < s.size() && s[i2] >= '0' && s[i2] <= '9'; ++i2) ;
	  if (i2 - i1 > 4) {
	    error(loc, "Invalid format width in interpolated string");
	    return nullptr;
	  }
	  width = std::stoi(s.substr(i0, i2 - i0));
	  i0 = i2;
	}
	if (i0 < s.size() && s[i0] == '.') {
	  ++i0;
	  size_t i1;
	  for (i1 = i0; i1 < s.size() && s[i1] >= '0' && s[i1] <= '9'; ++i1) ;
	  if (i1 - i0 > 4) {
	    error(loc, "Invalid format precision in interpolated string");
	    return nullptr;
	  }
	  precision = std::stoi(s.substr(i0, i1 - i0));
	  i0 = i1;
	}
	if (i0 < s.size() && s[i0] != '}') {
	  format = s[i0];
	  ++i0;
	}
      }
      if (i0 >= s.size() || s[i0] != '}') {
	error(loc, "Missing '}' in interpolated string");
	return nullptr;
      }
      ++i0;
      parts.push_back(std::make_unique<InterpStringArg>(loc, std::move(expr),
							width, precision, format));
    } else {
      std::string chars;
      do {
	char c = s[i0++];
	if (c == '\\') {
	  if (i0 >= s.size()) {
	    error(loc, "End of input in interpolated string");
	    return nullptr;
	  }
	  c = s[i0++];
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
	  case '{':
	  case '}':
	  case '\\':
	    break;
	  default:
	    error(loc, "Invalid escape char in interpolated string");
	    return nullptr;
	  }
	}
	chars.push_back(c);
      } while (i0 < s.size() && s[i0] != '{');
      parts.push_back(std::make_unique<InterpStringChars>(loc, chars));
    }
  }
  return std::make_unique<InterpStringExpr>(loc, std::move(parts));
}

//------------------------------------------------------------------------

// Parse a non-grouping binary operator expression:
//   Parent = Child Op Child
//          | Child
// parseChild is the parser member function that parses a child.
// opInfoVec is the vector of valid operators.
std::unique_ptr<Expr> Parser::parseBinary(std::unique_ptr<Expr> (Parser::*parseChild)(),
					  const std::vector<ParseBinaryOpInfo> &opInfoVec) {
  Location loc = lexer.get(0).loc();
  std::unique_ptr<Expr> lhs = (this->*parseChild)();
  if (!lhs) {
    return nullptr;
  }
  Token tok = lexer.get(0);
  for (const ParseBinaryOpInfo &opInfo: opInfoVec) {
    if (opInfo.token == tok.kind()) {
      lexer.shift();
      std::unique_ptr<Expr> rhs = (this->*parseChild)();
      if (!rhs) {
	return nullptr;
      }
      lhs = std::make_unique<BinaryOpExpr>(loc, opInfo.op, std::move(lhs), std::move(rhs));
    }
  }
  return lhs;
}

// Parse a left-grouping binary operator expression:
//   Parent = Parent Op Child
//          | Child
// parseChild is the parser member function that parses a child.
// opInfoVec is the vector of valid operators.
std::unique_ptr<Expr> Parser::parseLeftBinary(std::unique_ptr<Expr> (Parser::*parseChild)(),
					      const std::vector<ParseBinaryOpInfo> &opInfoVec) {
  Location loc = lexer.get(0).loc();
  std::unique_ptr<Expr> lhs = (this->*parseChild)();
  if (!lhs) {
    return nullptr;
  }
  bool done;
  do {
    done = true;
    Token tok = lexer.get(0);
    for (const ParseBinaryOpInfo &opInfo: opInfoVec) {
      if (opInfo.token == tok.kind()) {
	lexer.shift();
	std::unique_ptr<Expr> rhs = (this->*parseChild)();
	if (!rhs) {
	  return nullptr;
	}
	lhs = std::make_unique<BinaryOpExpr>(loc, opInfo.op, std::move(lhs), std::move(rhs));
	done = false;
      }
    }
  } while (!done);
  return lhs;
}

// Parse a left-side unary operator expression:
//   Parent = Op Parent
//          | Child
// parseChild is the parser member function that parses a child.
// opInfoVec is the list of valid operators.
std::unique_ptr<Expr> Parser::parseLeftUnary(std::unique_ptr<Expr> (Parser::*parseChild)(),
					     const std::vector<ParseUnaryOpInfo> &opInfoVec) {
  Token tok = lexer.get(0);
  Location loc = tok.loc();
  for (const ParseUnaryOpInfo &opInfo: opInfoVec) {
    if (opInfo.token == tok.kind()) {
      lexer.shift();
      std::unique_ptr<Expr> child = parseLeftUnary(parseChild, opInfoVec);
      if (!child) {
	return nullptr;
      }
      return std::make_unique<UnaryOpExpr>(loc, opInfo.op, std::move(child));
    }
  }
  return (this->*parseChild)();
}

// Parse a list of zero/one or more elements:
//   List = List Elem
//        | Elem
//        | (empty)
// Elem is the element type.
// parseElem is the parser member function that parses an element
//   (which must return null on error).
// end is the token that ends the list
//   (but is not consumed by this function).
// If [allowEmpty] is true, an empty list is allowed.
template<class Elem>
bool Parser::parseList(std::unique_ptr<Elem> (Parser::*parseElem)(),
		       Token::Kind end, bool allowEmpty,
		       std::vector<std::unique_ptr<Elem>> &v) {
  if (allowEmpty && lexer.get(0).is(end)) {
    return true;
  }
  do {
    std::unique_ptr<Elem> elem = (this->*parseElem)();
    if (!elem) {
      return false;
    }
    v.push_back(std::move(elem));
  } while (!lexer.get(0).is(end));
  return true;
}

// Parse a separated list of zero/one or more elements:
//   List = List Separator Elem
//        | Elem
//        | (empty)
// Elem is the element type.
// parseElem is the parser member function that parses an element
//   (which must return null on error).
// separator is the token that separates list elements.
// end is the token that ends the list
//   (but is not consumed by this function).
// If [allowEmpty] is true, an empty list is allowed.
template<class Elem>
bool Parser::parseSepList(std::unique_ptr<Elem> (Parser::*parseElem)(),
			  Token::Kind separator, Token::Kind end,
			  bool allowEmpty,
			  std::vector<std::unique_ptr<Elem>> &v) {
  if (allowEmpty && lexer.get(0).is(end)) {
    return true;
  }
  while (true) {
    std::unique_ptr<Elem> elem = (this->*parseElem)();
    if (!elem) {
      return false;
    }
    v.push_back(std::move(elem));
    if (!lexer.get(0).is(separator)) {
      break;
    }
    lexer.shift();
  }
  return true;
}

// Expect a token of [kind]. If found: shift and return true. Else:
// return false. Does not emit an error.
bool Parser::expect(Token::Kind kind) {
  Token tok = lexer.get(0);
  if (!tok.is(kind)) {
    return false;
  }
  lexer.shift();
  return true;
}

// Expect a token of [kind]. If found: store the token string in
// [str], shift, and return true. Else: return false. Does not emit an
// error.
bool Parser::expect(Token::Kind kind, std::string &str) {
  Token tok = lexer.get(0);
  if (!tok.is(kind)) {
    return false;
  }
  str = tok.str();
  lexer.shift();
  return true;
}
