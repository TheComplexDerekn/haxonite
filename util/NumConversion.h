//========================================================================
//
// NumConversion.h
//
// Conversions between strings and numbers.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef NumConversion_h
#define NumConversion_h

#include <stdint.h>
#include <string>

//------------------------------------------------------------------------

// Convert a string to a signed 56-bit integer. Assumes the string is
// syntactically correct (optional minus sign, followed by one or more
// valid digits for the radix). Returns true on success, false if the
// value is out of bounds. [radix] must be in [2, 16].
extern bool stringToInt56(const std::string &s, int radix, int64_t &out);

// Same as stringToInt56() but also checks for valid integer syntax,
// returning false if [s] is not a valid integer.
extern bool stringToInt56Checked(const std::string &s, int radix, int64_t &out);

// Convert a string to a 32-bit float. Assumes the string is
// syntactically correct.
extern void stringToFloat(const std::string &s, float &out);

// Same as stringToFloat() but also checks for valid floating point
// syntax, returning false if [s] is not a valid float.
extern bool stringToFloatChecked(const std::string &s, float &out);

// Convert a 56-bit integer to a string. [precision] is treated as per
// the interpolated string spec. [radix] must be in [2, 16]. Radixes
// other than 10 are treated as unsigned.
extern std::string int56ToString(int64_t val, int radix, int precision);

// Convert a 32-bit float to a string. [format] and [precision] are
// treated as per the interpolated string spec:
//
// format  description  precision
// ------  -----------  -----------------------------------------------
// f       fixed        digits after decimal point
// F       fixed        digits after decimal point, trim trailing zeros
// e       exponential  digits after decimal point
// E       exponential  digits after decimal point, trim trailing zeros
// g       flexible     significant digits
// G|none  flexible     significant digits, trim trailing zeros
//
// If precision<0 (which means no precision was given), use full
// precision; in which case there are no zeros to trim, so [feg] are
// equivalent to [FEG].
extern std::string floatToString(float val, uint8_t format, int precision);

#endif // NumConversion_h
