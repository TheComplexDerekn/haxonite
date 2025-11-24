//========================================================================
//
// AST.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "AST.h"

//------------------------------------------------------------------------

static std::string space(int indent) {
  return std::string(indent * 2, ' ');
}

//------------------------------------------------------------------------

std::string Module::toString(int indent) {
  std::string s = space(indent) + "module " + name;
  if (!params.empty()) {
    s += "[";
    for (size_t i = 0; i < params.size(); ++i) {
      if (i > 0) {
	s += ",";
      }
      s += params[i];
    }
    s += "]";
  }
  s += " is\n\n";
  for (size_t i = 0; i < imports.size(); ++i) {
    s += imports[i]->toString(indent + 1);
  }
  s += "\n";
  for (size_t i = 0; i < elems.size(); ++i) {
    s += elems[i]->toString(indent + 1);
  }
  s += space(indent) + "end\n";
  return s;
}

//------------------------------------------------------------------------

std::string Import::toString(int indent) {
  return space(indent) + "import " + name + "\n";
}

//------------------------------------------------------------------------

std::string StructDefn::toString(int indent) {
  std::string s = space(indent);
  if (pub) {
    s += "public ";
  }
  s += "struct " + name + " is\n";
  for (size_t i = 0; i < fields.size(); ++i) {
    s += fields[i]->toString(indent + 1);
  }
  s += space(indent) + "end\n\n";
  return s;
}

//------------------------------------------------------------------------

std::string VarStructDefn::toString(int indent) {
  std::string s = space(indent);
  if (pub) {
    s += "public ";
  }
  s += "varstruct " + name + " is\n";
  for (size_t i = 0; i < fields.size(); ++i) {
    s += fields[i]->toString(indent + 1);
  }
  for (size_t i = 0; i < subStructs.size(); ++i) {
    s += subStructs[i]->toString(indent + 1);
  }
  s += space(indent) + "end\n\n";
  return s;
}

//------------------------------------------------------------------------

std::string SubStructDefn::toString(int indent) {
  std::string s = space(indent) + "substruct " + name + " is\n";
  for (size_t i = 0; i < fields.size(); ++i) {
    s += fields[i]->toString(indent + 1);
  }
  s += space(indent) + "end\n";
  return s;
}

//------------------------------------------------------------------------

std::string Field::toString(int indent) {
  return space(indent) + name + ": " + type->toString() + ";\n";
}

//------------------------------------------------------------------------

std::string EnumDefn::toString(int indent) {
  std::string s = space(indent);
  if (pub) {
    s += "public ";
  }
  s += "enum " + name + " is\n";
  for (const std::string &member : members) {
    s += space(indent + 1) + member + ";\n";
  }
  s += space(indent) + "end\n";
  return s;
}

//------------------------------------------------------------------------

std::string NativeTypeDefn::toString(int indent) {
  std::string s = space(indent);
  if (pub) {
    s += "public ";
  }
  s += "nativetype ";
  for (size_t i = 0; i < attrs.size(); ++i) {
    if (i > 0) {
      s += ", ";
    }
    s += "\"" + attrs[i] + "\"";
  }
  s += " " + name + ";\n\n";
  return s;
}

//------------------------------------------------------------------------

std::string ConstDefn::toString(int indent) {
  std::string s = space(indent);
  if (pub) {
    s += "public ";
  }
  s += "const " + name + " = " + val->toString() + ";\n";
  return s;
}

//------------------------------------------------------------------------

std::string FuncDefn::toString(int indent) {
  std::string s = space(indent);
  if (pub) {
    s += "public ";
  }
  if (native) {
    s += "nativefunc ";
  } else {
    s += "func ";
  }
  s += name + "(";
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0) {
      s += ", ";
    }
    s += args[i]->toString();
  }
  s += ")";
  if (returnType) {
    s += " -> " + returnType->toString();
  }
  if (block) {
    s += " is\n";
    s += block->toString(indent + 1);
    s += space(indent) + "end\n\n";
  } else {
    s += ";\n\n";
  }
  return s;
}

//------------------------------------------------------------------------

std::string Arg::toString(int indent) {
  return name + ": " + type->toString();
}

//------------------------------------------------------------------------

std::string SimpleTypeRef::toString(int indent) {
  return name;
}

//------------------------------------------------------------------------

std::string ParamTypeRef::toString(int indent) {
  std::string s = name + "[";
  size_t n = params.size();
  for (size_t i = 0; i < n; ++i) {
    if (hasReturnType && i == n - 1) {
      s += "->";
    } else if (i > 0) {
      s += ",";
    }
    s += params[i]->toString();
  }
  s += "]";
  return s;
}

//------------------------------------------------------------------------

std::string TypeVarRef::toString(int indent) {
  return "$" + name;
}

//------------------------------------------------------------------------

std::string Block::toString(int indent) {
  std::string s;
  for (size_t i = 0; i < stmts.size(); ++i) {
    s += stmts[i]->toString(indent);
  }
  return s;
}

//------------------------------------------------------------------------

std::string VarStmt::toString(int indent) {
  return space(indent) + "var " + name + " = " + expr->toString() + ";\n";
}

//------------------------------------------------------------------------

std::string IfStmt::toString(int indent) {
  std::string s;
  for (size_t i = 0; i < tests.size(); ++i) {
    if (i == 0) {
      s += space(indent) + "if ";
    } else {
      s += space(indent) + "elseif ";
    }
    s += tests[i]->toString() + " then\n";
    s += blocks[i]->toString(indent + 1);
  }
  if (elseBlock) {
    s += space(indent) + "else\n";
    s += elseBlock->toString(indent + 1);
  }
  s += space(indent) + "end\n";
  return s;
}

//------------------------------------------------------------------------

std::string WhileStmt::toString(int indent) {
  std::string s = space(indent) + "while " + test->toString() + " do\n";
  s += block->toString(indent + 1);
  s += space(indent) + "end\n";
  return s;
}

//------------------------------------------------------------------------

std::string ForStmt::toString(int indent) {
  std::string s = space(indent) + "for " + var + " : " + expr1->toString();
  if (expr2) {
    s += " .. " + expr2->toString();
  }
  s += " do\n";
  s += block->toString(indent + 1);
  s += space(indent) + "end\n";
  return s;
}

//------------------------------------------------------------------------

std::string BreakStmt::toString(int indent) {
  return space(indent) + "break;\n";
}

//------------------------------------------------------------------------

std::string ContinueStmt::toString(int indent) {
  return space(indent) + "continue;\n";
}

//------------------------------------------------------------------------

std::string TypematchStmt::toString(int indent) {
  std::string s = space(indent) + "typematch " + expr->toString() + " is\n";
  for (std::unique_ptr<TypematchCase> &c : cases) {
    s += c->toString(indent + 1);
  }
  s += space(indent) + "end\n";
  return s;
}

//------------------------------------------------------------------------

std::string TypematchCase::toString(int indent) {
  std::string s = space(indent);
  if (type) {
    s += "case " + var + ": " + type->toString() + ":\n";
  } else {
    s += "default:\n";
  }
  s += block->toString(indent + 1);
  return s;
}

//------------------------------------------------------------------------

std::string ReturnStmt::toString(int indent) {
  if (expr) {
    return space(indent) + "return " + expr->toString() + ";\n";
  } else {
    return space(indent) + "return;\n";
  }
}

//------------------------------------------------------------------------

std::string AssignStmt::toString(int indent) {
  return space(indent) + lhs->toString() + " = " + rhs->toString() + ";\n";
}

//------------------------------------------------------------------------

std::string ExprStmt::toString(int indent) {
  return space(indent) + expr->toString() + ";\n";
}

//------------------------------------------------------------------------

std::string BinaryOpExpr::toString(int indent) {
  std::string s = lhs->toString();
  switch (op) {
  case BinaryOp::condOr:  s += " || ";  break;
  case BinaryOp::condAnd: s += " && ";  break;
  case BinaryOp::orOp:    s += " | ";   break;
  case BinaryOp::xorOp:   s += " ^ ";   break;
  case BinaryOp::andOp:   s += " & ";   break;
  case BinaryOp::eq:      s += " == ";  break;
  case BinaryOp::ne:      s += " != ";  break;
  case BinaryOp::same:    s += " === "; break;
  case BinaryOp::notSame: s += " !== "; break;
  case BinaryOp::lt:      s += " < ";   break;
  case BinaryOp::gt:      s += " > ";   break;
  case BinaryOp::le:      s += " <= ";  break;
  case BinaryOp::ge:      s += " >= ";  break;
  case BinaryOp::shl:     s += " << ";  break;
  case BinaryOp::shr:     s += " >> ";  break;
  case BinaryOp::add:     s += " + ";   break;
  case BinaryOp::sub:     s += " - ";   break;
  case BinaryOp::mul:     s += " * ";   break;
  case BinaryOp::div:     s += " / ";   break;
  case BinaryOp::mod:     s += " % ";   break;
  }
  s += rhs->toString();
  return s;
}

//------------------------------------------------------------------------

std::string UnaryOpExpr::toString(int indent) {
  std::string s;
  switch (op) {
  case UnaryOp::neg:       s += "-"; break;
  case UnaryOp::notOp:     s += "!"; break;
  case UnaryOp::length:    s += "#"; break;
  case UnaryOp::varstruct: s += "varstruct "; break;
  case UnaryOp::substruct: s += "substruct "; break;
  }
  s += expr->toString();
  return s;
}

//------------------------------------------------------------------------

std::string PropagateExpr::toString(int indent) {
  return expr->toString() + "?";
}

//------------------------------------------------------------------------

std::string CheckExpr::toString(int indent) {
  return expr->toString() + "!";
}

//------------------------------------------------------------------------

std::string CallExpr::toString(int indent) {
  std::string s = func->toString() + "(";
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0) {
      s += ", ";
    }
    s += args[i]->toString();
  }
  s += ")";
  return s;
}

//------------------------------------------------------------------------

std::string MemberExpr::toString(int indent) {
  return lhs->toString() + "." + member;
}

//------------------------------------------------------------------------

std::string IndexExpr::toString(int indent) {
  return obj->toString() + "[" + idx->toString() + "]";
}

//------------------------------------------------------------------------

std::string ParenExpr::toString(int indent) {
  return "(" + expr->toString() + ")";
}

//------------------------------------------------------------------------

std::string NewExpr::toString(int indent) {
  return "new " + type->toString();
}

//------------------------------------------------------------------------

std::string MakeExpr::toString(int indent) {
  std::string s = "make " + type->toString() + "(";
  for (size_t i = 0; i < fieldInits.size(); ++i) {
    if (i > 0) {
      s += ", ";
    }
    s += fieldInits[i]->toString();
  }
  s += ")";
  return s;
}

//------------------------------------------------------------------------

std::string FieldInit::toString(int indent) {
  return name + ": " + val->toString();
}

//------------------------------------------------------------------------

std::string FuncPointerExpr::toString(int indent) {
  std::string s = "&" + name + "(";
  for (size_t i = 0; i < argTypes.size(); ++i) {
    if (i > 0) {
      s += ",";
    }
    s += argTypes[i]->toString();
  }
  s += ")";
  return s;
}

//------------------------------------------------------------------------

std::string NilExpr::toString(int indent) {
  return "nil[" + type->toString() + "]";
}

//------------------------------------------------------------------------

std::string NilTestExpr::toString(int indent) {
  return "nil(" + expr->toString() + ")";
}

//------------------------------------------------------------------------

std::string ErrorExpr::toString(int indent) {
  std::string s = "error[";
  if (type) {
    s += type->toString();
  }
  s += "]";
  return s;
}

//------------------------------------------------------------------------

std::string ValidExpr::toString(int indent) {
  std::string s = "valid(";
  if (expr) {
    s += expr->toString();
  }
  s += ")";
  return s;
}

//------------------------------------------------------------------------

std::string OkExpr::toString(int indent) {
  return "ok(" + expr->toString() + ")";
}

//------------------------------------------------------------------------

std::string IdentExpr::toString(int indent) {
  return name;
}

//------------------------------------------------------------------------

std::string LitVectorExpr::toString(int indent) {
  std::string s = "[";
  for (size_t i = 0; i < vals.size(); ++i) {
    if (i > 0) {
      s += ", ";
    }
    s += vals[i]->toString();
  }
  s += "]";
  return s;
}

//------------------------------------------------------------------------

std::string LitSetExpr::toString(int indent) {
  std::string s = "{";
  for (size_t i = 0; i < vals.size(); ++i) {
    if (i > 0) {
      s += ", ";
    }
    s += vals[i]->toString();
  }
  s += "}";
  return s;
}

//------------------------------------------------------------------------

std::string LitMapExpr::toString(int indent) {
  std::string s = "{";
  for (size_t i = 0; i < pairs.size(); ++i) {
    if (i > 0) {
      s += ", ";
    }
    s += pairs[i].first->toString();
    s += ":";
    s += pairs[i].second->toString();
  }
  s += "}";
  return s;
}

//------------------------------------------------------------------------

std::string LitIntExpr::toString(int indent) {
  return val;
}

//------------------------------------------------------------------------

std::string LitFloatExpr::toString(int indent) {
  return val;
}

//------------------------------------------------------------------------

std::string LitBoolExpr::toString(int indent) {
  return val ? "true" : "false";
}

//------------------------------------------------------------------------

std::string LitCharExpr::toString(int indent) {
  std::string s = "'";
  char c = s[0];
  if (c == '\n') {
    s += "\\n";
  } else if (c == '\r') {
    s += "\\r";
  } else if (c == '\t') {
    s += "\\t";
  } else if (c == '\'' || c == '\\') {
    s += '\\';
    s += c;
  } else {
    s += c;
  }
  s += '\'';
  return s;
}

//------------------------------------------------------------------------

std::string LitStringExpr::toString(int indent) {
  std::string s = "\"";
  for (char c : val) {
    if (c == '\n') {
      s += "\\n";
    } else if (c == '\r') {
      s += "\\r";
    } else if (c == '\t') {
      s += "\\t";
    } else if (c == '"' || c == '\\') {
      s += '\\';
      s += c;
    } else {
      s += c;
    }
  }
  s += '"';
  return s;
}

//------------------------------------------------------------------------

std::string InterpStringExpr::toString(int indent) {
  std::string s = "$\"";
  for (std::unique_ptr<InterpStringPart> &part : parts) {
    s += part->toString();
  }
  s += '"';
  return s;
}

//------------------------------------------------------------------------

std::string InterpStringChars::toString(int indent) {
  std::string s;
  for (char c : chars) {
    if (c == '\n') {
      s += "\\n";
    } else if (c == '\r') {
      s += "\\r";
    } else if (c == '\t') {
      s += "\\t";
    } else if (c == '"' || c == '{' || c == '}' || c == '\\') {
      s += '\\';
      s += c;
    } else {
      s += c;
    }
  }
  return s;
}

//------------------------------------------------------------------------

std::string InterpStringArg::toString(int indent) {
  std::string s = "{" + expr->toString();
  if (width != 0 || precision != -1 || format != '\0') {
    s += ':';
    if (width != 0) {
      s += std::to_string(width);
    }
    if (precision >= 0) {
      s += '.';
      s += std::to_string(precision);
    }
    if (format != '\0') {
      s += format;
    }
  }
  s += '}';
  return s;
}
