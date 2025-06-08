//========================================================================
//
// NumConversion.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "NumConversion.h"
#include <string.h>
#include <algorithm>
#include <double-conversion/double-conversion.h>

using namespace double_conversion;

//------------------------------------------------------------------------

static void trimTrailingZeros(char *buf);

//------------------------------------------------------------------------

static int charToDigit[256] = {
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 1x
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 2x
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1, // 3x
   0, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 4x
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 5x
   0, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 6x
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 7x
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 8x
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 9x
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ax
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // bx
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // cx
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // dx
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ex
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1  // fx
};

static char digitToChar[17] = "0123456789abcdef";

//------------------------------------------------------------------------

bool stringToInt56(const std::string &s, int radix, int64_t &out) {
  size_t i = 0;
  bool neg;
  uint64_t maxVal;
  if (s[i] == '-') {
    neg = true;
    ++i;
  } else {
    neg = false;
  }
  if (radix != 10) {
    maxVal = UINT64_C(0x00ffffffffffffff);
  } else if (neg) {
    maxVal = UINT64_C(0x0080000000000000);
  } else {
    maxVal = UINT64_C(0x007fffffffffffff);
  }
  uint64_t x = 0;
  for (; i < s.size(); ++i) {
    int digit = charToDigit[s[i] & 0xff];
    if (x > (maxVal - digit) / radix) {
      return false;
    }
    x = x * radix + digit;
  }
  out = neg ? -(int64_t)x : (int64_t)x;
  return true;
}

bool stringToInt56Checked(const std::string &s, int radix, int64_t &out) {
  if (radix < 2 || radix > 16) {
    return false;
  }
  size_t i = 0;
  if (s[i] == '-') {
    ++i;
  }
  for (; i < s.size(); ++i) {
    int digit = charToDigit[s[i] & 0xff];
    if (digit < 0 || digit >= radix) {
      return false;
    }
  }
  return stringToInt56(s, radix, out);
}

void stringToFloat(const std::string &s, float &out) {
  out = std::stof(s);
}

bool stringToFloatChecked(const std::string &s, float &out) {
  // -? [0-9]* .? [0-9]* ([Ee][+-]?[0-9]+)?
  // (there must be at least one digit before the E)
  size_t i = 0;
  if (s[i] == '-') {
    ++i;
  }
  size_t nDigits = 0;
  while (s[i] >= '0' && s[i] <= '9') {
    ++nDigits;
    ++i;
  }
  if (s[i] == '.') {
    ++i;
  }
  while (s[i] >= '0' && s[i] <= '9') {
    ++nDigits;
    ++i;
  }
  if (nDigits == 0) {
    return false;
  }
  if (s[i] == 'E' || s[i] == 'e') {
    ++i;
    if (s[i] == '+' || s[i] == '-') {
      ++i;
    }
    nDigits = 0;
    while (s[i] >= '0' && s[i] <= '9') {
      ++nDigits;
      ++i;
    }
    if (nDigits == 0) {
      return false;
    }
  }
  if (i != s.size()) {
    return false;
  }
  stringToFloat(s, out);
  return true;
}

std::string int56ToString(int64_t val, int radix, int precision) {
  bool neg;
  uint64_t x;
  int minLength;
  if (radix == 10 && val < 0) {
    neg = true;
    x = (uint64_t)-val;
    minLength = std::max(1, precision - 1);
  } else {
    neg = false;
    x = (uint64_t)val;
    minLength = std::max(1, precision);
  }

  std::string out;
  do {
    int digit = x % radix;
    out.push_back(digitToChar[digit]);
    x /= radix;
  } while (x > 0);

  int length = (int)out.size();
  if (length < minLength) {
    out.insert(out.size(), minLength - length, '0');
  }

  if (neg) {
    out.push_back('-');
  }

  std::reverse(out.begin(), out.end());

  return out;
}

static_assert(DoubleToStringConverter::kMaxFixedDigitsBeforePoint == 60,
	      "unexpected value for DoubleToStringConverter::kMaxFixedDigitsBeforePoint");
static_assert(DoubleToStringConverter::kMaxFixedDigitsAfterPoint == 100,
	      "unexpected value for DoubleToStringConverter::kMaxFixedDigitsAfterPoint");
static_assert(DoubleToStringConverter::kMaxExponentialDigits == 120,
	      "unexpected value for DoubleToStringConverter::kMaxExponentialDigits");
static_assert(DoubleToStringConverter::kMaxPrecisionDigits == 120,
	      "unexpected value for DoubleToStringConverter::kMaxPrecisionDigits");

std::string floatToString(float val, uint8_t format, int precision) {
  // given the assertions above, the max string size should be 129
  // (kMaxExponentialDigits + 8 + the null byte)
  char buf[150];
  StringBuilder sb(buf, sizeof(buf));

  bool ok;
  if (precision < 0) {
    switch (format) {

    case 'f':
    case 'F': {
      double_conversion::DoubleToStringConverter cvt(
	  0,
	  "inf", "nan", 'e', -400, 400, 0, 0);
      ok = cvt.ToShortestSingle(val, &sb);
      sb.Finalize();
      break;
    }

    case 'e':
    case 'E': {
      double_conversion::DoubleToStringConverter cvt(
	  0,
	  "inf", "nan", 'e', 1, 0, 0, 0);
      ok = cvt.ToShortestSingle(val, &sb);
      sb.Finalize();
      break;
    }

    case 'g':
    case 'G':
    default: {
      double_conversion::DoubleToStringConverter cvt(
	  0,
	  "inf", "nan", 'e', -4, 6, 0, 0);
      ok = cvt.ToShortestSingle(val, &sb);
      sb.Finalize();
      break;
    }

    }

  } else {
    switch (format) {

    case 'f':
    case 'F': {
      double_conversion::DoubleToStringConverter cvt(
	  0,
	  "inf", "nan", 'e', 0, 0, 0, 0);
      ok = cvt.ToFixed(val, precision, &sb);
      sb.Finalize();
      if (format == 'F') {
	trimTrailingZeros(buf);
      }
      break;
    }

    case 'e':
    case 'E': {
      double_conversion::DoubleToStringConverter cvt(
	  0,
	  "inf", "nan", 'e', 0, 0, 0, 0);
      ok = cvt.ToExponential(val, precision, &sb);
      sb.Finalize();
      if (format == 'E') {
	trimTrailingZeros(buf);
      }
      break;
    }

    case 'g': {
      double_conversion::DoubleToStringConverter cvt(
	  0,
	  "inf", "nan", 'e', 0, 0, 4, std::max(0, 5 - precision));
      ok = cvt.ToPrecision(val, precision, &sb);
      sb.Finalize();
      break;
    }

    case 'G':
    default: {
      double_conversion::DoubleToStringConverter cvt(
	  DoubleToStringConverter::NO_TRAILING_ZERO,
	  "inf", "nan", 'e', 0, 0, 4, std::max(0, 5 - precision));
      ok = cvt.ToPrecision(val, precision, &sb);
      sb.Finalize();
      break;
    }

    }
  }

  if (!ok) {
    return "?";
  }
  return buf;
}

// Remove trailing zeros after the decimal point.  Also removes the
// decimal point if there are no non-zero digits after it.
static void trimTrailingZeros(char *buf) {
  //  decPt   end
  //  v       v
  // 1.2300000e+4
  //    ^
  //    lastNonZero

  //  decPt   end
  //  v       v
  // 1.2300000
  //    ^
  //    lastNonZero

  //  decPt   end
  //  v       v
  // 3.0000000
  // ^
  // lastNonZero

  if (!*buf) {
    return;
  }

  char *decPt = strchr(buf, '.');
  if (!decPt) {
    return;
  }

  char *end;
  for (end = decPt + 1; *end && *end != 'e'; ++end) ;

  char *lastNonZero;
  for (lastNonZero = end - 1;
       lastNonZero > decPt && *lastNonZero == '0';
       --lastNonZero);
  if (lastNonZero == decPt) {
    --lastNonZero;
  }

  char *p = lastNonZero + 1;
  char *q = end;
  while (*q) {
    *p++ = *q++;
  }
  *p = '\0';
}
