#pragma once
#include <string.h>
struct Preferences { void begin(const char *, bool) {} uint8_t getUChar(const char *, uint8_t d) { return d; } void putUChar(const char *, uint8_t) {} uint32_t getUInt(const char *, uint32_t d) { return d; } void putUInt(const char *, uint32_t) {}
  size_t getString(const char *, char *v, size_t) { v[0] = 0; return 0; } size_t putString(const char *, const char *) { return 0; } };
