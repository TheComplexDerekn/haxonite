//========================================================================
//
// FileNames.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "FileNames.h"

//------------------------------------------------------------------------

std::string makeSourceDirPath(const std::string &dir) {
  return dir + "/src";
}

std::string makeHeaderDirPath(const std::string &dir) {
  return dir + "/hdr";
}

std::string makeObjectDirPath(const std::string &dir) {
  return dir + "/obj";
}

std::string makeBinDirPath(const std::string &dir) {
  return dir + "/bin";
}

std::string makeSourceFileName(const std::string &dir, const std::string &moduleName) {
  return makeSourceDirPath(dir) + "/" + moduleName + ".hax";
}

std::string makeHeaderFileName(const std::string &dir, const std::string &moduleName) {
  return makeHeaderDirPath(dir) + "/" + moduleName + ".haxh";
}

std::string makeShortFileName(const std::string &dir, const std::string &moduleName,
			      bool isHeader) {
  size_t i = dir.rfind('/');
  if (i == std::string::npos) {
    i = 0;
  } else {
    ++i;
  }
  return dir.substr(i) + "/src/" + moduleName + (isHeader ? ".haxh" : ".hax");
}

std::string makeObjectFileName(const std::string &dir, const std::string &moduleName) {
  return makeObjectDirPath(dir) + "/" + moduleName + ".haxo";
}

std::string makeExecutableFileName(const std::string &dir, const std::string &moduleName) {
  return makeBinDirPath(dir) + "/" + moduleName + ".haxe";
}
