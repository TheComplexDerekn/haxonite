//========================================================================
//
// ConfigFile.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "ConfigFile.h"
#include "SysIO.h"

//------------------------------------------------------------------------

ConfigFile::ConfigFile() {
}

bool ConfigFile::load(const std::string &path,
		      std::function<void(int lineNum, const std::string &msg)> err) {
  Parser parser;
  if (!readFile(path, parser.buf)) {
    return true;
  }
  parser.pos = 0;
  parser.lineNum = 1;

  //--- file header
  if (!more(parser)) {
    err(0, "Missing config file header");
    return false;
  }
  std::vector<std::string> line = readLine(parser, err);
  if (line.size() != 1 || line[0] != "@haxonite-config-1") {
    err(parser.lineNum - 1, "Invalid config file header");
    return false;
  }

  //--- sections and items
  Section *section = nullptr;
  while (more(parser)) {
    line = readLine(parser, err);
    if (line.empty()) {
      return false;
    }

    //--- section
    if (line.size() == 1 && line[0][0] == '-') {
      std::string tag = line[0].substr(1);
      sections[tag] = std::unique_ptr<Section>(new Section());
      section = sections[tag].get();

    //--- item
    } else {
      if (!section) {
	err(parser.lineNum - 1, "Missing section header");
	return false;
      }
      std::string cmd = line[0];
      Item *item = new Item();
      for (size_t i = 1; i < line.size(); ++i) {
	item->args.push_back(line[i]);
      }
      section->items[cmd] = std::unique_ptr<Item>(item);
    }
  }

  return true;
}

bool ConfigFile::more(Parser &parser) {
  while (true) {
    // end of data
    if (parser.pos >= parser.buf.size()) {
      return false;
    }
    size_t pos2 = parser.pos;

    // skip spaces at start of line
    while (pos2 < parser.buf.size() && parser.buf[pos2] == ' ') {
      ++pos2;
    }

    // skip comment
    if (pos2 + 2 <= parser.buf.size() && parser.buf[pos2] == '/' && parser.buf[pos2+1] == '/') {
      pos2 += 2;
      while (pos2 < parser.buf.size() && parser.buf[pos2] != '\n') {
	++pos2;
      }
    }

    // end of data (unterminated last line)
    if (pos2 >= parser.buf.size()) {
      return false;

    // blank line or comment
    } else if (parser.buf[pos2] == '\n') {
      parser.pos = pos2 + 1;
      ++parser.lineNum;

    // valid line
    } else {
      return true;
    }
  }
}

std::vector<std::string> ConfigFile::readLine(
			     Parser &parser,
			     std::function<void(int lineNum, const std::string &msg)> err) {
  std::vector<std::string> tokens;

  // skip spaces at start of line
  while (parser.pos < parser.buf.size() && parser.buf[parser.pos] == ' ') {
    ++parser.pos;
  }
  if (parser.pos == parser.buf.size()) {
    err(parser.lineNum, "Syntax error");
    return tokens;
  }

  while (true) {
    char c = parser.buf[parser.pos];

    // end of line
    if (c == '\n') {
      ++parser.pos;
      ++parser.lineNum;
      break;
    }

    std::string token;

    // quoted token
    if (c == '"') {
      ++parser.pos;
      while (true) {
	if (parser.pos == parser.buf.size()) {
	  err(parser.lineNum, "Unterminated string");
	  tokens.clear();
	  return tokens;
	}
	c = parser.buf[parser.pos];
	++parser.pos;
	if (c == '"') {
	  break;
	}
	if (c == '\\') {
	  if (parser.pos == parser.buf.size()) {
	    err(parser.lineNum, "Invalid escape character in string");
	    tokens.clear();
	    return tokens;
	  }
	  c = parser.buf[parser.pos];
	  ++parser.pos;
	  if (c == '"' || c == '\\') {
	    token += c;
	  } else {
	    err(parser.lineNum, "Invalid escape character in string");
	    tokens.clear();
	    return tokens;
	  }
	} else {
	  token += c;
	}
      }

    // unquoted token
    } else {
      while (parser.pos < parser.buf.size()) {
	c = parser.buf[parser.pos];
	if (c == ' ' || c == '\n') {
	  break;
	}
	token += c;
	++parser.pos;
      }
    }

    tokens.push_back(token);

    // skip spaces before next token
    while (parser.pos < parser.buf.size() && parser.buf[parser.pos] == ' ') {
      ++parser.pos;
    }

    // unterminated line
    if (parser.pos == parser.buf.size()) {
      // increment lineNum here because parseDataFile() error messages use lineNum - 1
      ++parser.lineNum;
      break;
    }
  }

  return tokens;
}

ConfigFile::Item *ConfigFile::item(const std::string &sectionTag, const std::string &cmd) {
  auto sectionIter = sections.find(sectionTag);
  if (sectionIter == sections.end()) {
    return nullptr;
  }
  Section *section = sectionIter->second.get();
  auto itemIter = section->items.find(cmd);
  if (itemIter == section->items.end()) {
    return nullptr;
  }
  return itemIter->second.get();
}
