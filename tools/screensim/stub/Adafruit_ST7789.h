#pragma once
#include "Adafruit_GFX.h"
#define ST77XX_BLACK 0
#define ST77XX_RED 0xF800
struct SPIClass { void begin(int, int, int) {} };
static SPIClass SPI;
struct Adafruit_ST7789 {
  Adafruit_ST7789(SPIClass *, int, int, int) {}
  void init(int, int) {} void setSPISpeed(uint32_t) {} void setRotation(int) {} void fillScreen(uint16_t) {}
  void setTextColor(uint16_t) {} void setCursor(int, int) {} void print(const char *) {}
  void drawRGBBitmap(int, int, uint16_t *, int, int) {}
};
