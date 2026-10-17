#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <algorithm>
#define PROGMEM
#define pgm_read_byte(a) (*(const uint8_t *)(a))
#define pgm_read_word(a) (*(const uint16_t *)(a))
#define pgm_read_dword(a) (*(const uintptr_t *)(a))
#define PI 3.14159265358979f
#define LOW 0
#define HIGH 1
#define INPUT_PULLUP 2
typedef bool boolean;
typedef uint8_t byte;
extern uint32_t g_millis;
inline uint32_t millis() { return g_millis; }
inline void delay(uint32_t) {}
inline int digitalRead(int) { return HIGH; }
inline void pinMode(int, int) {}
inline void ledcAttach(int, int, int) {}
inline void ledcWrite(int, int) {}
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif
class __FlashStringHelper;
struct String { const char *c_str() const { return ""; } unsigned length() const { return 0; } };
#define radians(d) ((d) * PI / 180.0f)
#include <time.h>
inline struct tm *gmtime_r(const time_t *t, struct tm *r) { gmtime_s(r, t); return r; }
#include "Print.h"
struct SerialStub { void begin(int) {} int available() { return 0; } int read() { return -1; } void println(const char *) {} };
static SerialStub Serial;
