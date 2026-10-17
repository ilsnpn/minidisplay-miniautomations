// Hello World — Waveshare ESP32-C6-LCD-1.47
// Écran ST7789 172x320 (SPI) + LED RGB sur GPIO8

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_NeoPixel.h>

// Brochage (voir docs/HARDWARE.md)
#define PIN_MOSI 6
#define PIN_SCLK 7
#define PIN_MISO 5
#define PIN_LCD_CS 14
#define PIN_LCD_DC 15
#define PIN_LCD_RST 21
#define PIN_LCD_BL 22
#define PIN_RGB 8

Adafruit_ST7789 tft(&SPI, PIN_LCD_CS, PIN_LCD_DC, PIN_LCD_RST);
Adafruit_NeoPixel rgb(1, PIN_RGB, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);

  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH);

  SPI.begin(PIN_SCLK, PIN_MISO, PIN_MOSI);
  tft.init(172, 320);  // décalage colonne (240-172)/2 = 34 calculé par la lib
  tft.setSPISpeed(40000000);
  tft.setRotation(1);  // paysage : 320x172
  tft.fillScreen(ST77XX_BLACK);

  // Cadre pour vérifier les bords / l'offset
  tft.drawRect(0, 0, tft.width(), tft.height(), ST77XX_CYAN);

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(4);
  tft.setCursor(25, 50);
  tft.print("Hello World");

  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(70, 110);
  tft.print("MiniDisplay C6");

  rgb.begin();
  rgb.setBrightness(40);

  Serial.println("Hello World - MiniDisplay pret");
}

void loop() {
  // Cycle de couleurs sur la LED RGB
  static uint16_t hue = 0;
  rgb.setPixelColor(0, rgb.gamma32(rgb.ColorHSV(hue)));
  rgb.show();
  hue += 256;
  delay(20);
}
