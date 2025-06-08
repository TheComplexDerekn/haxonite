# Part of the Haxonite project, under the MIT License.
# Copyright 2025 Derek Noonburg

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${PROJECT_SOURCE_DIR}/bytecode")
include_directories("${PROJECT_SOURCE_DIR}/util")

add_library(compiler
  ${CMAKE_CURRENT_BINARY_DIR}/AST.h
  AST.cpp
  BuiltinTypes.cpp
  CodeGenBlock.cpp
  CodeGenExpr.cpp
  CodeGenFunc.cpp
  CodeGenModule.cpp
  ConstExpr.cpp
  Context.cpp
  CTree.cpp
  Error.cpp
  FileNames.cpp
  FunctionChecker.cpp
  Instantiator.cpp
  Lexer.cpp
  Link.cpp
  Mangle.cpp
  ModuleScanner.cpp
  Parser.cpp
  TypeCheck.cpp
  TypeRefConnector.cpp
)

add_executable(haxc haxc.cpp)
target_link_libraries(haxc compiler bytecode util
                      double-conversion)

add_executable(lexertest lexertest.cpp)
target_link_libraries(lexertest compiler util
                      double-conversion)

add_executable(parsertest parsertest.cpp)
target_link_libraries(parsertest compiler util
                      double-conversion)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/AST.h
  DEPENDS AST.in ${CMAKE_CURRENT_SOURCE_DIR}/make-ast
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/make-ast <${CMAKE_CURRENT_SOURCE_DIR}/AST.in >AST.h
)

