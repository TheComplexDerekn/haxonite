//========================================================================
//
// DateTime.h
//
// A DateTime object represents a point in time.
//
// DateTime is an immutable value class.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef DateTime_h
#define DateTime_h

#include <time.h>

//------------------------------------------------------------------------

class DateTime {
public:

  // Create an invalid DateTime object.
  DateTime(): mT(0), mNS(-1) {}

  // Create a DateTime object from a Unix time and an optional
  // nanosecond count.
  DateTime(time_t aT, int aNS = 0): mT(aT), mNS(aNS) {}

  // Return true if this DateTime is valid.
  bool valid() { return mNS >= 0; }

  // Returns:
  //   -1 if [this] is earlier than [other]
  //    0 if [this] is equal to [other]
  //   +1 if [this] is later than [other]
  int cmp(DateTime other);

private:

  time_t mT;			// seconds from Unix epoch
  int mNS;			// nanoseconds (negative means invalid DateTime)
};

#endif // DateTime_h
