//========================================================================
//
// UTF8.cpp
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#include "UTF8.h"

//------------------------------------------------------------------------

bool utf8Get(const uint8_t *s, int64_t length, int64_t &idx, uint32_t &u) {
  if (idx < 0 || idx >= length) {
    u = 0;
    return false;
  }

  // default for invalid (and single-byte) UTF-8 sequences is to
  // return the first byte
  uint32_t c0 = s[idx++];
  u = c0;

  if (c0 < 0x80) {
    // done
  } else if (c0 < 0xe0 && idx <= INT64_MAX - 1 && idx + 1 <= length) {
    if ((s[idx] & 0xc0) == 0x80) {
      u = ((c0 & 0x1f) << 6) | (s[idx] & 0x3f);
      ++idx;
    }
  } else if (c0 < 0xf0 && idx <= INT64_MAX - 2 && idx + 2 <= length) {
    if ((s[idx] & 0xc0) == 0x80 &&
        (s[idx+1] & 0xc0) == 0x80) {
      u = ((c0 & 0x0f) << 12) |
          ((s[idx] & 0x3f) << 6) |
          (s[idx+1] & 0x3f);
      idx += 2;
    }
  } else if (c0 < 0xf8 && idx <= INT64_MAX - 3 && idx + 3 <= length) {
    if ((s[idx] & 0xc0) == 0x80 &&
        (s[idx+1] & 0xc0) == 0x80 &&
        (s[idx+2] & 0xc0) == 0x80) {
      u = ((c0 & 0x07) << 18) |
          ((s[idx] & 0x3f) << 12) |
          ((s[idx+1] & 0x3f) << 6) |
          (s[idx+2] & 0x3f);
      idx += 3;
    }
  } else if (c0 < 0xfc && idx <= INT64_MAX - 4 && idx + 4 <= length) {
    if ((s[idx] & 0xc0) == 0x80 &&
        (s[idx+1] & 0xc0) == 0x80 &&
        (s[idx+2] & 0xc0) == 0x80 &&
        (s[idx+3] & 0xc0) == 0x80) {
      u = ((c0 & 0x03) << 24) |
          ((s[idx] & 0x3f) << 18) |
          ((s[idx+1] & 0x3f) << 12) |
          ((s[idx+2] & 0x3f) << 6) |
          (s[idx+3] & 0x3f);
      idx += 4;
    }
  } else if (c0 < 0xfe && idx <= INT64_MAX - 5 && idx + 5 <= length) {
    if ((s[idx] & 0xc0) == 0x80 &&
        (s[idx+1] & 0xc0) == 0x80 &&
        (s[idx+2] & 0xc0) == 0x80 &&
        (s[idx+3] & 0xc0) == 0x80 &&
        (s[idx+4] & 0xc0) == 0x80) {
      u = ((c0 & 0x01) << 30) |
          ((s[idx] & 0x3f) << 24) |
          ((s[idx+1] & 0x3f) << 18) |
          ((s[idx+2] & 0x3f) << 12) |
          ((s[idx+3] & 0x3f) << 6) |
          (s[idx+4] & 0x3f);
      idx += 5;
    }
  }
  return true;
}

int utf8Length(const uint8_t *s, int64_t length, int64_t idx) {
  if (idx < 0 || idx >= length) {
    return 0;
  }
  uint32_t c0 = s[idx];
  if (c0 < 0x80) {
    return 1;
  } else if (c0 < 0xe0 && idx <= INT64_MAX - 1 && idx + 1 <= length) {
    if ((s[idx+1] & 0xc0) == 0x80) {
      return 2;
    }
  } else if (c0 < 0xf0 && idx <= INT64_MAX - 2 && idx + 2 <= length) {
    if ((s[idx+1] & 0xc0) == 0x80 &&
        (s[idx+2] & 0xc0) == 0x80) {
      return 3;
    }
  } else if (c0 < 0xf8 && idx <= INT64_MAX - 3 && idx + 3 <= length) {
    if ((s[idx+1] & 0xc0) == 0x80 &&
        (s[idx+2] & 0xc0) == 0x80 &&
        (s[idx+3] & 0xc0) == 0x80) {
      return 4;
    }
  } else if (c0 < 0xfc && idx <= INT64_MAX - 4 && idx + 4 <= length) {
    if ((s[idx+1] & 0xc0) == 0x80 &&
        (s[idx+2] & 0xc0) == 0x80 &&
        (s[idx+3] & 0xc0) == 0x80 &&
        (s[idx+4] & 0xc0) == 0x80) {
      return 5;
    }
  } else if (c0 < 0xfe && idx <= INT64_MAX - 5 && idx + 5 <= length) {
    if ((s[idx+1] & 0xc0) == 0x80 &&
        (s[idx+2] & 0xc0) == 0x80 &&
        (s[idx+3] & 0xc0) == 0x80 &&
        (s[idx+4] & 0xc0) == 0x80 &&
        (s[idx+5] & 0xc0) == 0x80) {
      return 6;
    }
  }
  return 1;
}

int utf8Encode(uint32_t u, uint8_t *out) {
  if (u < 0x80) {
    out[0] = (char)u;
    return 1;
  } else if (u < 0x800) {
    out[0] = (char)(0xc0 + (u >> 6));
    out[1] = (char)(0x80 + (u & 0x3f));
    return 2;
  } else if (u < 0x10000) {
    out[0] = (char)(0xe0 + (u >> 12));
    out[1] = (char)(0x80 + ((u >> 6) & 0x3f));
    out[2] = (char)(0x80 + (u & 0x3f));
    return 3;
  } else if (u < 0x200000) {
    out[0] = (char)(0xf0 + (u >> 18));
    out[1] = (char)(0x80 + ((u >> 12) & 0x3f));
    out[2] = (char)(0x80 + ((u >> 6) & 0x3f));
    out[3] = (char)(0x80 + (u & 0x3f));
    return 4;
  } else if (u < 0x4000000) {
    out[0] = (char)(0xf8 + (u >> 24));
    out[1] = (char)(0x80 + ((u >> 18) & 0x3f));
    out[2] = (char)(0x80 + ((u >> 12) & 0x3f));
    out[3] = (char)(0x80 + ((u >> 6) & 0x3f));
    out[4] = (char)(0x80 + (u & 0x3f));
    return 5;
  } else if (u < 0x80000000) {
    out[0] = (char)(0xfc + (u >> 30));
    out[1] = (char)(0x80 + ((u >> 24) & 0x3f));
    out[2] = (char)(0x80 + ((u >> 18) & 0x3f));
    out[3] = (char)(0x80 + ((u >> 12) & 0x3f));
    out[4] = (char)(0x80 + ((u >> 6) & 0x3f));
    out[5] = (char)(0x80 + (u & 0x3f));
    return 6;
  } else {
    return 0;
  }
}

