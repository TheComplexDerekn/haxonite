//========================================================================
//
// Location.h
//
// Source file location.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef Location_h
#define Location_h

#include <memory>
#include <string>

//------------------------------------------------------------------------

class Location {
public:

  Location(): mLine(0) {}
  Location(std::shared_ptr<std::string> aPath): mPath(aPath), mLine(0) {}
  Location(std::shared_ptr<std::string> aPath, int aLine): mPath(aPath), mLine(aLine) {}
  bool hasPath() { return (bool)mPath; }
  std::string path() { return *mPath; }
  bool hasLine() { return mLine != 0; }
  int line() { return mLine; }

private:

  std::shared_ptr<std::string> mPath;
  int mLine;
};

#endif // Location_h
