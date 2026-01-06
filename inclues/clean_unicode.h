#pragma once
#include <string>

// Clean unsupported Unicode (e.g., U+2010 true hyphen) -> normal '-'
inline std::string clean_unicode(const std::string &input) {
  std::string s = input;
  for (size_t i = 0; i + 2 < s.length();) {
    // UTF‑8 for U+2010 is E2 80 90
    if ((unsigned char)s[i] == 0xE2 &&
        (unsigned char)s[i+1] == 0x80 &&
        (unsigned char)s[i+2] == 0x90) {
      s.replace(i, 3, "-");
      i += 1;
    } else {
      i += 1;
    }
  }
  return s;
}
