//========================================================================
//
// runtime_datetime.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "runtime_datetime.h"
#include <time.h>
#include <sys/time.h>
#include "BytecodeDefs.h"

//------------------------------------------------------------------------

struct Date {
  uint64_t hdr;
  Cell year;
  Cell month;
  Cell day;
};

#define dateNCells (sizeof(Date) / sizeof(Cell) - 1)

struct DateTime {
  uint64_t hdr;
  Cell year;
  Cell month;
  Cell day;
  Cell hour;
  Cell minute;
  Cell second;
  Cell nanosecond;
  Cell tz;
};

#define dateTimeNCells (sizeof(DateTime) / sizeof(Cell) - 1)

struct Timestamp {
  uint64_t hdr;
  Cell seconds;
  Cell nanoseconds;
};

#define timestampNCells (sizeof(Timestamp) / sizeof(Cell) - 1)

#define tzLocal 2000

//------------------------------------------------------------------------

// From: https://c-faq.com/lib/calendar.br.html
static int64_t daysElapsed(int64_t year, int64_t month, int64_t day) {
  static int64_t monthDays[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  if (month < 1 || month > 12) {
    BytecodeEngine::fatalError("Invalid argument");
  }
  int64_t n = 365 * (year - 1);
  int64_t y = (month < 3) ? (year - 1) : year;
  return n + y/4 - y/100 + y/400 + monthDays[month - 1] + day;
}

//------------------------------------------------------------------------

// toDateTime(ts: Timestamp, tz: Int) -> DateTime
static NativeFuncDefn(runtime_toDateTime_9TimestampI) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &tsCell = engine.arg(0);
  Cell &tzCell = engine.arg(1);

  Timestamp *ts = (Timestamp *)cellPtr(tsCell);
  engine.failOnNilPtr(ts);
  int64_t tz = cellInt(tzCell);

  struct tm tm;
  if (tz == tzLocal) {
    time_t t = (time_t)cellInt(ts->seconds);
    localtime_r(&t, &tm);
    tz = tm.tm_gmtoff / 60;
  } else {
    time_t t = (time_t)(cellInt(ts->seconds) + tz * 60);
    gmtime_r(&t, &tm);
  }
  int64_t ns = cellInt(ts->nanoseconds);

  // NB: this may trigger GC
  DateTime *dt = (DateTime *)engine.heapAllocTuple(dateTimeNCells, 0);
  dt->year = cellMakeInt(tm.tm_year + 1900);
  dt->month = cellMakeInt(tm.tm_mon + 1);
  dt->day = cellMakeInt(tm.tm_mday);
  dt->hour = cellMakeInt(tm.tm_hour);
  dt->minute = cellMakeInt(tm.tm_min);
  dt->second = cellMakeInt(tm.tm_sec);
  dt->nanosecond = cellMakeInt(ns);
  dt->tz = cellMakeInt(tz);

  engine.push(cellMakeHeapPtr(dt));
}

static int64_t toTimestampSeconds(DateTime *dt) {
  struct tm tm = {0};
  tm.tm_year = cellInt(dt->year) - 1900;
  tm.tm_mon = cellInt(dt->month) - 1;
  tm.tm_mday = cellInt(dt->day);
  tm.tm_hour = cellInt(dt->hour);
  tm.tm_min = cellInt(dt->minute);
  tm.tm_sec = cellInt(dt->second);
  tm.tm_isdst = -1;
  return (int64_t)timegm(&tm) - cellInt(dt->tz) * 60;
}

// toTimestamp(dt: DateTime) -> Timestamp
static NativeFuncDefn(runtime_toTimestamp_8DateTime) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &dtCell = engine.arg(0);

  DateTime *dt = (DateTime *)cellPtr(dtCell);
  engine.failOnNilPtr(dt);

  int64_t s = toTimestampSeconds(dt);
  int64_t ns = cellInt(dt->nanosecond);

  // NB: this may trigger GC
  engine.push(timestampMake(s, ns, engine));
}

// now() -> Timestamp
static NativeFuncDefn(runtime_now) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 0) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif

  timeval tv;
  gettimeofday(&tv, nullptr);

  // NB: this may trigger GC
  engine.push(timestampMake(tv.tv_sec, (int64_t)tv.tv_usec * 1000, engine));
}

// dayOfWeek(d: Date) -> Int
static NativeFuncDefn(runtime_dayOfWeek_4Date) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &dCell = engine.arg(0);

  Date *d = (Date *)cellPtr(dCell);
  engine.failOnNilPtr(d);

  int64_t dw = daysElapsed(cellInt(d->year), cellInt(d->month), cellInt(d->day)) % 7;
  engine.push(cellMakeInt(dw));
}

// dayOfWeek(dt: DateTime) -> Int
static NativeFuncDefn(runtime_dayOfWeek_8DateTime) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 1 ||
      !cellIsPtr(engine.arg(0))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &dtCell = engine.arg(0);

  DateTime *dt = (DateTime *)cellPtr(dtCell);
  engine.failOnNilPtr(dt);

  int64_t dw = daysElapsed(cellInt(dt->year), cellInt(dt->month), cellInt(dt->day)) % 7;
  engine.push(cellMakeInt(dw));
}

// compare(d1: Date, d2: Date) -> Int
static NativeFuncDefn(runtime_compare_4Date4Date) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &d1Cell = engine.arg(0);
  Cell &d2Cell = engine.arg(1);

  Date *d1 = (Date *)cellPtr(d1Cell);
  engine.failOnNilPtr(d1);
  Date *d2 = (Date *)cellPtr(d2Cell);
  engine.failOnNilPtr(d2);

  int64_t cmp;
  if (cellInt(d1->year) < cellInt(d2->year)) {
    cmp = -1;
  } else if (cellInt(d1->year) > cellInt(d2->year)) {
    cmp = 1;
  } else if (cellInt(d1->month) < cellInt(d2->month)) {
    cmp = -1;
  } else if (cellInt(d1->month) > cellInt(d2->month)) {
    cmp = 1;
  } else if (cellInt(d1->day) < cellInt(d2->day)) {
    cmp = -1;
  } else if (cellInt(d1->day) > cellInt(d2->day)) {
    cmp = 1;
  } else {
    cmp = 0;
  }

  engine.push(cellMakeInt(cmp));
}

// compare(dt1: DateTime, dt2: DateTime) -> Int
static NativeFuncDefn(runtime_compare_8DateTime8DateTime) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &dt1Cell = engine.arg(0);
  Cell &dt2Cell = engine.arg(1);

  DateTime *dt1 = (DateTime *)cellPtr(dt1Cell);
  engine.failOnNilPtr(dt1);
  DateTime *dt2 = (DateTime *)cellPtr(dt2Cell);
  engine.failOnNilPtr(dt2);

  int64_t s1 = toTimestampSeconds(dt1);
  int64_t s2 = toTimestampSeconds(dt2);
  int64_t ns1 = cellInt(dt1->nanosecond);
  int64_t ns2 = cellInt(dt2->nanosecond);

  int64_t cmp;
  if (s1 < s2) {
    cmp = -1;
  } else if (s1 > s2) {
    cmp = 1;
  } else if (ns1 < ns2) {
    cmp = -1;
  } else if (ns1 > ns2) {
    cmp = 1;
  } else {
    cmp = 0;
  }

  engine.push(cellMakeInt(cmp));
}

// compare(ts1: Timestamp, ts2: Timestamp) -> Int
static NativeFuncDefn(runtime_compare_9Timestamp9Timestamp) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &ts1Cell = engine.arg(0);
  Cell &ts2Cell = engine.arg(1);

  Timestamp *ts1 = (Timestamp *)cellPtr(ts1Cell);
  engine.failOnNilPtr(ts1);
  Timestamp *ts2 = (Timestamp *)cellPtr(ts2Cell);
  engine.failOnNilPtr(ts2);

  int64_t cmp;
  if (cellInt(ts1->seconds) < cellInt(ts2->seconds)) {
    cmp = -1;
  } else if (cellInt(ts1->seconds) > cellInt(ts2->seconds)) {
    cmp = 1;
  } else if (cellInt(ts1->nanoseconds) < cellInt(ts2->nanoseconds)) {
    cmp = -1;
  } else if (cellInt(ts1->nanoseconds) > cellInt(ts2->nanoseconds)) {
    cmp = 1;
  } else {
    cmp = 0;
  }

  engine.push(cellMakeInt(cmp));
}

// diff(d1: Date, d2: Date) -> Int
static NativeFuncDefn(runtime_diff_4Date4Date) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &d1Cell = engine.arg(0);
  Cell &d2Cell = engine.arg(1);

  Date *d1 = (Date *)cellPtr(d1Cell);
  engine.failOnNilPtr(d1);
  Date *d2 = (Date *)cellPtr(d2Cell);
  engine.failOnNilPtr(d2);

  int64_t delta = daysElapsed(cellInt(d2->year), cellInt(d2->month), cellInt(d2->day)) -
                  daysElapsed(cellInt(d1->year), cellInt(d1->month), cellInt(d1->day));
  engine.push(cellMakeInt(delta));
}

// diff(ts1: Timestamp, ts2: Timestamp) -> Int
static NativeFuncDefn(runtime_diff_9Timestamp9Timestamp) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &ts1Cell = engine.arg(0);
  Cell &ts2Cell = engine.arg(1);

  Timestamp *ts1 = (Timestamp *)cellPtr(ts1Cell);
  engine.failOnNilPtr(ts1);
  Timestamp *ts2 = (Timestamp *)cellPtr(ts2Cell);
  engine.failOnNilPtr(ts2);

  engine.push(cellMakeInt(cellInt(ts2->seconds) - cellInt(ts1->seconds)));
}

// diffNS(ts1: Timestamp, ts2: Timestamp) -> Int
static NativeFuncDefn(runtime_diffNS_9Timestamp9Timestamp) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsPtr(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &ts1Cell = engine.arg(0);
  Cell &ts2Cell = engine.arg(1);

  Timestamp *ts1 = (Timestamp *)cellPtr(ts1Cell);
  engine.failOnNilPtr(ts1);
  Timestamp *ts2 = (Timestamp *)cellPtr(ts2Cell);
  engine.failOnNilPtr(ts2);

  int64_t deltaS = cellInt(ts2->seconds) - cellInt(ts1->seconds);
  int64_t deltaNS = cellInt(ts2->nanoseconds) - cellInt(ts1->nanoseconds);
  if (deltaS > (bytecodeMaxInt - deltaNS) / 1000000000 ||
      deltaS < (bytecodeMinInt - deltaNS) / 1000000000) {
    engine.fatalError("Integer overflow");
  }
  engine.push(cellMakeInt(deltaS * 1000000000 + deltaNS));
}

// add(d: Date, days: Int) -> Date
static NativeFuncDefn(runtime_add_4DateI) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 2 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &dCell = engine.arg(0);
  Cell &daysCell = engine.arg(1);

  struct Date *d = (Date *)cellPtr(dCell);
  engine.failOnNilPtr(d);
  int64_t days = cellInt(daysCell);

  // mktime() normalizes the fields in tm
  struct tm tm = {0};
  tm.tm_year = cellInt(d->year) - 1900;
  tm.tm_mon = cellInt(d->month) - 1;
  tm.tm_mday = cellInt(d->day) + days;
  tm.tm_hour = 12;
  tm.tm_min = 0;
  tm.tm_sec = 0;
  tm.tm_isdst = -1;
  mktime(&tm);

  // NB: this may trigger GC
  struct Date *dOut = (Date *)engine.heapAllocTuple(dateNCells, 0);
  dOut->year = cellMakeInt(tm.tm_year + 1900);
  dOut->month = cellMakeInt(tm.tm_mon + 1);
  dOut->day = cellMakeInt(tm.tm_mday);

  engine.push(cellMakeHeapPtr(dOut));
}

// add(ts: Timestamp, seconds: Int, nanoseconds: Int) -> Timestamp
static NativeFuncDefn(runtime_add_9TimestampII) {
#if CHECK_RUNTIME_FUNC_ARGS
  if (engine.nArgs() != 3 ||
      !cellIsPtr(engine.arg(0)) ||
      !cellIsInt(engine.arg(1)) ||
      !cellIsInt(engine.arg(2))) {
    BytecodeEngine::fatalError("Invalid argument");
  }
#endif
  Cell &tsCell = engine.arg(0);
  Cell &secondsCell = engine.arg(1);
  Cell &nanosecondsCell = engine.arg(2);

  Timestamp *ts = (Timestamp *)cellPtr(tsCell);
  engine.failOnNilPtr(ts);
  int64_t seconds = cellInt(secondsCell);
  int64_t nanoseconds = cellInt(nanosecondsCell);

  int64_t sOut = cellInt(ts->seconds) + seconds;
  int64_t nsOut = cellInt(ts->nanoseconds) + nanoseconds;
  if (nsOut > 1000000000) {
    nsOut -= 1000000000;
    sOut += 1;
  } else if (nsOut < 0) {
    nsOut += 1000000000;
    sOut -= 1;
  }

  // NB: this may trigger GC
  engine.push(timestampMake(sOut, nsOut, engine));
}

//------------------------------------------------------------------------

void runtime_datetime_init(BytecodeEngine &engine) {
  engine.addNativeFunction("toDateTime_9TimestampI", &runtime_toDateTime_9TimestampI);
  engine.addNativeFunction("toTimestamp_8DateTime", &runtime_toTimestamp_8DateTime);
  engine.addNativeFunction("now", &runtime_now);
  engine.addNativeFunction("dayOfWeek_4Date", &runtime_dayOfWeek_4Date);
  engine.addNativeFunction("dayOfWeek_8DateTime", &runtime_dayOfWeek_8DateTime);
  engine.addNativeFunction("compare_4Date4Date", &runtime_compare_4Date4Date);
  engine.addNativeFunction("compare_8DateTime8DateTime", &runtime_compare_8DateTime8DateTime);
  engine.addNativeFunction("compare_9Timestamp9Timestamp", &runtime_compare_9Timestamp9Timestamp);
  engine.addNativeFunction("diff_4Date4Date", &runtime_diff_4Date4Date);
  engine.addNativeFunction("diff_9Timestamp9Timestamp", &runtime_diff_9Timestamp9Timestamp);
  engine.addNativeFunction("diffNS_9Timestamp9Timestamp", &runtime_diffNS_9Timestamp9Timestamp);
  engine.addNativeFunction("add_4DateI", &runtime_add_4DateI);
  engine.addNativeFunction("add_9TimestampII", &runtime_add_9TimestampII);
}

//------------------------------------------------------------------------
// support functions
//------------------------------------------------------------------------

Cell timestampMake(int64_t seconds, int64_t nanoseconds, BytecodeEngine &engine) {
  Timestamp *ts = (Timestamp *)engine.heapAllocTuple(timestampNCells, 0);
  ts->seconds = cellMakeInt(seconds);
  ts->nanoseconds = cellMakeInt(nanoseconds);
  return cellMakeHeapPtr(ts);
}
