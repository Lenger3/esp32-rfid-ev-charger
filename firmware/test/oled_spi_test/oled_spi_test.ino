/*
 * !!! KULLANILMIYOR — modul I2C moduna alindi. Sadece SPI'ye geri donulurse gerekir.
 *
 * M242-12864-7-V2.0  (SSD1309 2.42" 128x64, 7 pin) — SPI TEST
 * ESP32-WROOM-32U
 *
 * ---------------------------------------------------------------------------
 * BU MODUL FABRIKA CIKISI SPI'DIR
 * ---------------------------------------------------------------------------
 * M242-12864-7 / lcdwiki MSP242X ailesinde arayuz secimi 0R direncle yapilir:
 *     SPI  : R5 takili, R4 ve R9 YOK      <-- fabrika cikisi bu
 *     I2C  : R4 + R9 takili, R5 YOK       <-- 0603 rework gerekir
 * Bu yuzden I2C'ye gecmek icin modulu sokup lehim yapmak gerekir. Gerek yok:
 * SPI hem varsayilan, hem plaket yerlesimimizle (CS=5, DC=4) birebir uyumlu.
 *
 * ---------------------------------------------------------------------------
 * KABLOLAMA (7 pin, yukaridan asagiya)
 * ---------------------------------------------------------------------------
 *   pin 1  GND  -> GND
 *   pin 2  VCC  -> 3V3
 *   pin 3  SCL  -> GPIO 18   (SPI SCLK)
 *   pin 4  SDA  -> GPIO 23   (SPI MOSI)
 *   pin 5  RES  -> GPIO 16
 *   pin 6  DC   -> GPIO 4
 *   pin 7  CS   -> GPIO 5
 *
 *   MISO (GPIO 19) bu modulde KULLANILMAZ (ekran yazma-only).
 *   GPIO 19 RC522 icin serbest kalir.
 *
 * ---------------------------------------------------------------------------
 * KUTUPHANE: olikraus/U8g2   (PlatformIO: olikraus/U8g2@^2.35.30)
 * ---------------------------------------------------------------------------
 * BEKLENEN: seri portta pin dokumu, ekranda 5 sayfa dongusu.
 */

#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>

#define PIN_SCLK   18
#define PIN_MOSI   23
#define PIN_CS      5
#define PIN_DC      4
#define PIN_RES    16

// 4-wire hardware SPI. ESP32'de varsayilan SPI: SCK=18, MOSI=23, MISO=19
U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2(U8G2_R0,
                                            /* cs=*/   PIN_CS,
                                            /* dc=*/   PIN_DC,
                                            /* reset=*/PIN_RES);

// Ekran acilmazsa sirayla dene:
//   U8G2_SSD1309_128X64_NONAME2_F_4W_HW_SPI   (bazi paneller)
//   U8G2_SSD1309_128X64_NONAME0_1_4W_HW_SPI   (page buffer, RAM tasarrufu)
//   U8G2_SSD1309_128X64_NONAME0_F_4W_SW_SPI u8g2(U8G2_R0, PIN_SCLK, PIN_MOSI, PIN_CS, PIN_DC, PIN_RES);

void page1_hello() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB10_tf);
  u8g2.drawStr(0, 13, "MERHABA");
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 27, "M242-12864-7");
  u8g2.drawStr(0, 39, "SSD1309 128x64");
  u8g2.drawStr(0, 51, "SPI test OK");
  u8g2.drawStr(0, 63, "EVCHARGE-01");
  u8g2.sendBuffer();
}

void page2_grid() {
  u8g2.clearBuffer();
  for (int x = 0; x < 128; x += 8) u8g2.drawVLine(x, 0, 64);
  for (int y = 0; y < 64; y += 8) u8g2.drawHLine(0, y, 128);
  u8g2.setDrawColor(0);
  u8g2.drawBox(1, 1, 56, 13);
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(3, 11, "izgara 8px");
  u8g2.sendBuffer();
}

void page3_frames() {
  u8g2.clearBuffer();
  u8g2.drawFrame(0, 0, 128, 64);
  u8g2.drawFrame(4, 4, 120, 56);
  u8g2.drawRFrame(8, 8, 112, 48, 6);
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(22, 32, "CERCEVE");
  u8g2.drawDisc(112, 52, 6);
  u8g2.drawCircle(96, 52, 6);
  u8g2.sendBuffer();
}

void page4_contrast() {
  char buf[20];
  for (int c = 10; c <= 250; c += 20) {
    u8g2.setContrast(c);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(2, 16, "kontrast");
    snprintf(buf, sizeof(buf), "%d / 255", c);
    u8g2.setFont(u8g2_font_helvB12_tf);
    u8g2.drawStr(2, 38, buf);
    u8g2.drawBox(2, 46, (c * 12) / 25, 9);
    u8g2.sendBuffer();
    delay(110);
  }
  u8g2.setContrast(200);
}

void page5_stress() {
  u8g2.clearBuffer();
  for (int i = 0; i < 40; i++) {
    int x = random(0, 118), y = random(0, 56);
    int w = random(2, 12), h = random(2, 8);
    if (random(0, 2)) u8g2.drawBox(x, y, w, h);
    else u8g2.drawFrame(x, y, w, h);
  }
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\n\n=== M242-12864-7 / SSD1309 SPI TEST ===");
  Serial.printf("SCLK=%d  MOSI=%d  CS=%d  DC=%d  RES=%d\n",
                PIN_SCLK, PIN_MOSI, PIN_CS, PIN_DC, PIN_RES);
  Serial.println("Modul fabrika cikisi SPI (R5 takili, R4/R9 yok). I2C icin rework gerekir.");

  u8g2.begin();
  u8g2.setBusClock(8000000);     // 8 MHz; kararsizsa 4000000 yap
  u8g2.setContrast(200);
  Serial.println("u8g2 begin() tamam");
}

void loop() {
  page1_hello();    Serial.println("sayfa 1/5  MERHABA");   delay(2000);
  page2_grid();     Serial.println("sayfa 2/5  izgara");    delay(2000);
  page3_frames();   Serial.println("sayfa 3/5  cerceve");   delay(2000);
  page4_contrast(); Serial.println("sayfa 4/5  kontrast");  delay(400);
  page5_stress();   Serial.println("sayfa 5/5  stres");     delay(4000);
}
