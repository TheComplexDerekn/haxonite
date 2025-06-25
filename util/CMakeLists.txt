# Part of the Haxonite project, under the MIT License.
# Copyright 2025 Derek Noonburg

include_directories("${PROJECT_BINARY_DIR}/util")

add_library(util
  ConfigFile.cpp
  DateTime.cpp
  NumConversion.cpp
  SysIO.cpp
  UTF8.cpp
)
