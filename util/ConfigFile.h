//========================================================================
//
// ConfigFile.h
//
// System configuration file. This uses the same format as the
// Haxonite DataFile module.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef ConfigFile_h
#define ConfigFile_h

#include <functional>
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>

//------------------------------------------------------------------------

class ConfigFile {
public:

  struct Item {
    std::vector<std::string> args;
  };

  // Create an empty ConfigFile object.
  ConfigFile();

  // Load a config file. On success: returns true. On error: calls
  // [err] and returns false.
  bool load(const std::string &path, std::function<void(int lineNum, const std::string &msg)> err);

  // Get the item corresponding to [cmd] in section
  // [sectionTag]. Returns null if that item isn't present.
  Item *item(const std::string &sectionTag, const std::string &cmd);

private:

  struct Section {
    std::unordered_map<std::string, std::unique_ptr<Item>> items;
  };

  struct Parser {
    std::string buf;
    size_t pos;
    int lineNum;
  };

  bool more(Parser &parser);
  std::vector<std::string> readLine(Parser &parser,
				    std::function<void(int lineNum, const std::string &msg)> err);

  std::unordered_map<std::string, std::unique_ptr<Section>> sections;
};

#endif // ConfigFile_h
