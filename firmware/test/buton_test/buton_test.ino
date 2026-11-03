/*
 * ===========================================================================
 *  BUTON TESTI  —  hangi GPIO, kac ms basili, kac kez
 * ===========================================================================
 *
 *  Ne yapar:
 *   1. OLED'i baslatir (calisan tam-tarama yontemiyle)
 *   2. Tarama modu:  hangi GPIO'lar su an LOW (basili) — hepsini listeler
 *   3. Test modu:    4 butonu olcer, basili tuttugun sureyi ms cinsinden gosterir
 *
 *  Ekran calismasa bile SERI PORTTAN her sey okunur (115200).
 *
 *  Kullanim:
 *      pio run -t upload && pio device monitor
 *
 *  Ekran/buton durumu:
 *   - Ekran yoksa: seri cikti yine calisir, ekranda hicbir sey gorunmez.
 *   - Bir butona bas: ekranda o satir vurgulanir, sure sayaci isler.
 *   - Birak: "BAS Kibir: 342 ms" yazar, sayac artar.
 *
 *  MOD DEGISTIR:
 *   - 3 saniye hicbir tusa basmazsan TARAMA moduna gecer (tum GPIO'lar).
 *   - Tekrar basinca test moduna doner.
 */

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// --- OLED (kart_sistem ile ayni) ---
#define PIN_SDA  21
#define PIN_SCL  22

U8G2_SSD1309_128X64_NONAME0_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
bool oledOk = false;
byte oledAddr = 0x3C;

// --- test edilecek butonlar: (etiket, GPIO) ---
// gercek firmware board.h ile ayni olmali
struct BtnDef {
  const char* ad;
  int         pin;
};

BtnDef btns[4] = {
  {"EVET ", 26},
  {"HAYIR", 33},
  {"GERI ", 32},
  {"OK   ", 25},
};

// Durum
bool     down[4]      = {false, false, false, false};
uint32_t downAt[4]    = {0, 0, 0, 0};
uint32_t lastMs[4]    = {0, 0, 0, 0};
uint32_t maxMs[4]     = {0, 0, 0, 0};
int      count[4]     = {0, 0, 0, 0};
int      lastActive   = -1;
bool     scanMode     = false;
uint32_t lastActivity = 0;

// ============================================================ OLED
static int scanFull(byte* found) {
  int n = 0;
  for (byte a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      n++;
      Serial.printf("[i2c] 0x%02X\n", a);
      if ((a == 0x3C || a == 0x3D) && !*found) *found = a;
    }
  }
  return n;
}

bool oledBegin() {
  byte addr = 0;
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
  delay(40);

  int n = scanFull(&addr);
  Serial.printf("[i2c] tarama 1: %d cihaz, oled=%s\n", n, addr ? "var" : "yok");

  if (!addr) {
    Wire.end(); delay(25);
    Wire.begin(PIN_SCL, PIN_SDA);          // takas
    Wire.setClock(100000);
    delay(40);
    n = scanFull(&addr);
    Serial.printf("[i2c] tarama 2 (takas): %d cihaz, oled=%s\n", n, addr ? "var" : "yok");
    Wire.end(); delay(25);
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(100000);
    delay(40);
    if (!addr) scanFull(&addr);
  }

  if (!addr) {
    Serial.println("[i2c] OLED YOK — ekransiz devam, seri ciktiya bak");
    return false;
  }

  oledAddr = addr;
  oled.setI2CAddress(addr << 1);
  oled.begin();
  oled.setBusClock(400000);
  oled.setContrast(200);
  oled.setPowerSave(0);

  Wire.beginTransmission(addr);
  bool alive = (Wire.endTransmission() == 0);
  if (!alive) {
    delay(70);
    oled.begin();
    oled.setBusClock(400000);
    Wire.beginTransmission(addr);
    alive = (Wire.endTransmission() == 0);
  }
  Serial.printf("[i2c] 0x%02X: %s\n", addr, alive ? "ACK" : "NAK");
  return alive;
}

// ============================================================ cizim
void drawTest() {
  if (!oledOk) return;
  oled.clearBuffer();

  oled.setFont(u8g2_font_6x10_tf);
  oled.drawBox(0, 0, 128, 11);
  oled.setDrawColor(0);
  oled.drawStr(2, 9, "BUTON TESTI");
  char r[12];
  snprintf(r, sizeof(r), "0x%02X", oledAddr);
  oled.drawStr(96, 9, r);
  oled.setDrawColor(1);

  uint32_t now = millis();

  for (int i = 0; i < 4; i++) {
    int y = 21 + i * 10;
    bool on = down[i];

    if (on) {
      oled.drawBox(0, y - 8, 128, 10);
      oled.setDrawColor(0);
    }

    char left[20];
    snprintf(left, sizeof(left), "%-5s G%02d", btns[i].ad, btns[i].pin);
    oled.setFont(u8g2_font_5x7_tf);
    oled.drawStr(2, y, left);

    char right[16];
    if (on) {
      snprintf(right, sizeof(right), "%lu", (unsigned long)(now - downAt[i]));  // canli ms
    } else if (count[i]) {
      snprintf(right, sizeof(right), "%lu", (unsigned long)lastMs[i]);
    } else {
      snprintf(right, sizeof(right), down[i] ? "BAS" : "-");
    }
    oled.setFont(u8g2_font_6x10_tf);
    oled.drawStr(126 - oled.getStrWidth(right), y, right);

    oled.setDrawColor(1);
  }

  // alt bilgi
  oled.setFont(u8g2_font_5x7_tf);
  oled.drawHLine(0, 54, 128);
  char b[32];
  if (lastActive >= 0) {
    snprintf(b, sizeof(b), "%s  %lu ms  #%d",
             btns[lastActive].ad, (unsigned long)lastMs[lastActive],
             count[lastActive]);
  } else {
    snprintf(b, sizeof(b), "bir tusa bas...");
  }
  oled.drawStr(2, 62, b);
  oled.sendBuffer();
}

void drawScan() {
  if (!oledOk) return;
  oled.clearBuffer();

  oled.setFont(u8g2_font_6x10_tf);
  oled.drawBox(0, 0, 128, 11);
  oled.setDrawColor(0);
  oled.drawStr(2, 9, "GPIO TARAMASI");
  oled.setDrawColor(1);

  oled.setFont(u8g2_font_5x7_tf);
  oled.drawStr(2, 20, "basili olan pinler:");

  // basili olanlari listele
  int shown = 0;
  char buf[24];
  buf[0] = 0;
  for (int i = 0; i < 4; i++) {
    if (down[i]) {
      char t[8];
      snprintf(t, sizeof(t), "G%d ", btns[i].pin);
      strncat(buf, t, sizeof(buf) - strlen(buf) - 1);
      shown++;
    }
  }
  oled.setFont(u8g2_font_6x10_tf);
  if (shown) oled.drawStr(4, 34, buf);
  else       oled.drawStr(4, 34, "(hicbiri)");

  oled.setFont(u8g2_font_5x7_tf);
  oled.drawStr(2, 46, "test icin tusa bas");
  oled.sendBuffer();
}

// ============================================================ setup
void setup() {
  Serial.begin(115200);
  delay(600);
  Serial.println("\n\n==========================================");
  Serial.println("  BUTON TESTI");
  Serial.println("==========================================");

  // buton pinleri: INPUT_PULLUP, basinca LOW
  for (int i = 0; i < 4; i++) {
    pinMode(btns[i].pin, INPUT_PULLUP);
  }
  delay(80);

  // baslangic durumlari
  for (int i = 0; i < 4; i++) {
    bool low = (digitalRead(btns[i].pin) == LOW);
    Serial.printf("  %-5s GPIO %2d : %s\n", btns[i].ad, btns[i].pin,
                  low ? "LOW  <-- SIMDI BASILI!" : "HIGH (bosta, dogru)");
    down[i] = low;
    downAt[i] = millis();
  }

  Serial.println("------------------------------------------");

  oledOk = oledBegin();
  Serial.printf("  ekran: %s\n", oledOk ? "CALISIYOR" : "YOK");
  Serial.println("------------------------------------------");
  Serial.println("  her butona sirayla bas, sure ekranda/seride");
  Serial.println("==========================================\n");

  lastActivity = millis();
  drawTest();
}

// ============================================================ loop
void loop() {
  uint32_t now = millis();

  for (int i = 0; i < 4; i++) {
    bool low = (digitalRead(btns[i].pin) == LOW);

    if (low && !down[i]) {
      // basildi
      down[i]   = true;
      downAt[i] = now;
      lastActivity = now;
      if (scanMode) { scanMode = false; }
      Serial.printf("[BAS ] %-5s GPIO %2d\n", btns[i].ad, btns[i].pin);

    } else if (!low && down[i]) {
      // birakildi
      uint32_t dur = now - downAt[i];
      down[i]   = false;
      lastMs[i] = dur;
      if (dur > maxMs[i]) maxMs[i] = dur;
      count[i]++;
      lastActive = i;
      lastActivity = now;

      Serial.printf("[BIRAK] %-5s GPIO %2d  %lu ms  (%d. basis, ilk-temas %s)\n",
                    btns[i].ad, btns[i].pin, (unsigned long)dur, count[i],
                    dur < 30 ? "COK KISA" : "normal");
    }
  }

  // 3 sn hareketsizlik -> tarama modu
  if (!scanMode && (now - lastActivity) > 3000) {
    scanMode = true;
    Serial.println("[mod ] TARAMA (bir tusa basinca teste doner)");
    drawScan();
  }

  static uint32_t lastDraw = 0;
  if (now - lastDraw >= 50) {          // 20 fps: sure sayaci akici gorunsun
    lastDraw = now;
    if (scanMode) drawScan();
    else          drawTest();
  }

  // her 10 sn'de ozet
  static uint32_t lastSum = 0;
  if (now - lastSum >= 10000) {
    lastSum = now;
    Serial.printf("[ozet] EVET=%d HAYIR=%d GERI=%d OK=%d  |  max: %lu/%lu/%lu/%lu ms\n",
                  count[0], count[1], count[2], count[3],
                  (unsigned long)maxMs[0], (unsigned long)maxMs[1],
                  (unsigned long)maxMs[2], (unsigned long)maxMs[3]);
  }

  delay(5);
}
