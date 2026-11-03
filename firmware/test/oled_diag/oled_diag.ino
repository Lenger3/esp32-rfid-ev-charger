/*
 * ===========================================================================
 *  OLED TESHIS — "ekran bir kere cizip kaliyor" problemi icin
 *  ESP32-WROOM-32U · M242-12864-7-V2.0 (SSD1309, I2C)
 * ===========================================================================
 *
 *  Her adimda:
 *    - ekrana NE cizildigini buyuk harflerle basar
 *    - I2C hatti HALA CANLI MI diye ACK kontrolu yapar  <-- ASIL TESHIS
 *    - seri porta "ADIM x OK / BUS HUNG" yazar
 *
 *  Seri cikti okumasi:
 *    Tum adimlarda "bus=ACK"        -> I2C saglam, sorun cizim/font/koordinat
 *    Belli adimdan sonra "bus=NAK"  -> I2C kilitlendi (kablo/pull-up/hiz)
 *    Adim 3'te (beyaz ekran) bos     -> yazmalar hic ulasmiyor (rework/CS/A0)
 *    Adim 6'da hala bos              -> NONAME0/NONAME2 panel uyusmazligi
 */

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#define PIN_SDA 21
#define PIN_SCL 22
#define ADDR_RETRY 3

U8G2_SSD1309_128X64_NONAME0_F_HW_I2C d0(U8G2_R0, U8X8_PIN_NONE);
U8G2_SSD1309_128X64_NONAME2_F_HW_I2C d2(U8G2_R0, U8X8_PIN_NONE);

byte OLED_ADDR = 0x3C;
int busFails = 0;

// --- I2C hatti canli mi (ACK var mi) ---
bool busAck() {
  Wire.beginTransmission(OLED_ADDR);
  byte e = Wire.endTransmission();
  if (e != 0) busFails++;
  return e == 0;
}

// --- kilitlenmis I2C'yi kurtar: SCL'ye 9 darbe + STOP ---
void busRecover() {
  Serial.println("      [kurtarma] I2C bus reset deneniyor...");
  Wire.end();
  pinMode(PIN_SCL, OUTPUT);
  pinMode(PIN_SDA, INPUT_PULLUP);
  for (int i = 0; i < 9; i++) {
    digitalWrite(PIN_SCL, LOW);  delayMicroseconds(5);
    digitalWrite(PIN_SCL, HIGH); delayMicroseconds(5);
  }
  pinMode(PIN_SDA, OUTPUT);
  digitalWrite(PIN_SDA, LOW); delayMicroseconds(5);
  digitalWrite(PIN_SCL, HIGH); delayMicroseconds(5);
  digitalWrite(PIN_SDA, HIGH); delayMicroseconds(5);
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
  delay(50);
  Serial.printf("      [kurtarma] sonuc: %s\n", busAck() ? "ACK (kurtarildi)" : "hala NAK");
}

void step(int n, const char* what) {
  Serial.printf("\n--- ADIM %d: %s ---\n", n, what);
}

void reportBus(const char* tag) {
  bool ok = busAck();
  Serial.printf("   %-24s bus=%s%s\n", tag, ok ? "ACK" : "NAK",
                ok ? "" : "   <<< KILITLI");
  if (!ok) {
    busRecover();
  }
}

void bigText(U8G2& g, const char* l1, const char* l2) {
  g.clearBuffer();
  g.setFont(u8g2_font_helvB12_tf);
  int w = g.getStrWidth(l1);
  g.drawStr((128 - w) / 2, 26, l1);
  if (l2) {
    g.setFont(u8g2_font_6x10_tf);
    int w2 = g.getStrWidth(l2);
    g.drawStr((128 - w2) / 2, 46, l2);
  }
  g.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\n\n##################################################");
  Serial.println("#  OLED TESHIS — nerede takildigini bulur         #");
  Serial.println("##################################################");

  // ---- ADIM 0: hat bosta mi? (kablo/sicrama teshisi) ----
  step(0, "hat bosta mi? SDA/SCL seviyeleri");
  pinMode(PIN_SDA, INPUT_PULLUP);
  pinMode(PIN_SCL, INPUT_PULLUP);
  delay(20);
  int sdaIdle = digitalRead(PIN_SDA);
  int sclIdle = digitalRead(PIN_SCL);
  Serial.printf("   SDA(21)=%s  SCL(22)=%s\n",
                sdaIdle ? "HIGH" : "LOW", sclIdle ? "HIGH" : "LOW");
  if (!sdaIdle) Serial.println("   !! SDA surekli LOW -> bir cihaz hatti tutuyor veya kisa devre");
  if (!sclIdle) Serial.println("   !! SCL surekli LOW -> kisa devre veya ters baglanti");
  if (sdaIdle && sclIdle) Serial.println("   hat bosta, normal");

  // ---- ADIM 1: bus baslat, tara (iki pin sirasi da denenir) ----
  step(1, "I2C tarama — normal ve TAKAS pin sirasi");

  byte foundAddr = 0;
  bool swapped = false;

  for (int attempt = 0; attempt < 2 && !foundAddr; attempt++) {
    byte sdaPin = attempt == 0 ? PIN_SDA : PIN_SCL;
    byte sclPin = attempt == 0 ? PIN_SCL : PIN_SDA;
    Serial.printf("\n   deneme %d: SDA=%d SCL=%d\n", attempt + 1, sdaPin, sclPin);

    Wire.begin(sdaPin, sclPin);
    Wire.setClock(100000);
    delay(60);

    byte hits = 0;
    for (byte a = 1; a < 127; a++) {
      Wire.beginTransmission(a);
      if (Wire.endTransmission() == 0) {
        Serial.printf("     cihaz: 0x%02X\n", a);
        if ((a == 0x3C || a == 0x3D) && !foundAddr) foundAddr = a;
        hits++;
      }
    }
    if (hits == 0) Serial.println("     hicbir cihaz cevap vermedi");

    if (foundAddr && attempt == 1) swapped = true;
    Wire.end();
    delay(30);
  }

  if (!foundAddr) {
    Serial.println();
    Serial.println("   >>> OLED hic cevap vermiyor. Kod burada duruyor.");
    Serial.println();
    Serial.println("   SIRA ILE KONTROL (fiziksel):");
    Serial.println("     a) Loose kablo: SDA/SCL/VCC/GND hepsini cikar-tak");
    Serial.println("     b) CS  -> GND   (I2C modunda CS LOW olmali)  << EN SIK SEBEP");
    Serial.println("        CS havada kalirsa modul SPI moduna gecer, ACK vermez");
    Serial.println("     c) A0 (DC) -> GND  (adres 0x3C icin)");
    Serial.println("     d) RES -> 3V3");
    Serial.println("     e) VCC -> 3V3 olcu: 3.2-3.4 V");
    Serial.println("     f) R4 + R9 takili, R5 sokulu (rework)");
    Serial.println("     g) SDA/SCL kablolarini yer degistir");
    Serial.println();
    Serial.println("   Multimetre (modul soketinde, kart enerjili):");
    Serial.println("     VCC-GND = 3.3 V | CS-GND = 0 V | A0-GND = 0 V | RES-3V3 = 3.3 V");
    Serial.println();
    while (true) {
      delay(3000);
      pinMode(PIN_SDA, INPUT_PULLUP);
      pinMode(PIN_SCL, INPUT_PULLUP);
      Serial.printf("   [%lus] SDA=%s SCL=%s  (0x3C deneniyor...)\n",
                    millis() / 1000,
                    digitalRead(PIN_SDA) ? "H" : "L",
                    digitalRead(PIN_SCL) ? "H" : "L");
      Wire.begin(PIN_SDA, PIN_SCL);
      Wire.beginTransmission(0x3C);
      if (Wire.endTransmission() == 0) {
        Serial.println("   >>> 0x3C geri geldi! Yeniden baslat.");
        delay(300);
        ESP.restart();
      }
      Wire.end();
    }
  }

  OLED_ADDR = foundAddr;
  Serial.printf("\n   secilen adres: 0x%02X%s\n", OLED_ADDR,
                swapped ? "   (KABLOLAR TAKAS BAGLI!)" : "");
  if (swapped) {
    Serial.println("   >> SDA ve SCL yer degistirmis. Duzeltince yeniden baslat.");
  }

  // tarama sonrasi hatti normal siraya dondur
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
  delay(50);
  reportBus("tarama sonrasi");

  // ---- ADIM 2: u8g2 begin ----
  step(2, "u8g2 NONAME0 begin + kontrast");
  d0.setI2CAddress(OLED_ADDR << 1);
  d0.begin();
  d0.setBusClock(100000);
  d0.setContrast(200);
  d0.setPowerSave(0);
  reportBus("begin sonrasi");

  // ---- ADIM 3: BEYAZ EKRAN (en kritik test) ----
  step(3, "TAM BEYAZ ekran — yazmalar ulasiyor mu?");
  Serial.println("   EKRANA BAK: tamamen beyaz olmali.");
  for (int i = 0; i < 3; i++) {
    d0.clearBuffer();
    d0.drawBox(0, 0, 128, 64);       // her piksel acik
    d0.sendBuffer();
    reportBus("beyaz yazildi");
    delay(1200);
  }

  // ---- ADIM 4: TAM SIYAH ----
  step(4, "TAM SIYAH ekran");
  Serial.println("   EKRANA BAK: tamamen siyah olmali.");
  for (int i = 0; i < 2; i++) {
    d0.clearBuffer();
    d0.sendBuffer();
    reportBus("siyah yazildi");
    delay(1200);
  }

  // ---- ADIM 5: buyuk metin + cerceve ----
  step(5, "buyuk metin + cerceve");
  Serial.println("   EKRANA BAK: 'TEST' ve cerceve");
  for (int i = 0; i < 3; i++) {
    d0.clearBuffer();
    d0.drawFrame(0, 0, 128, 64);
    d0.setFont(u8g2_font_helvB12_tf);
    d0.drawStr(28, 38, "TEST");
    d0.sendBuffer();
    reportBus("metin yazildi");
    delay(1500);
  }

  // ---- ADIM 6: NONAME2 varyanti ----
  step(6, "NONAME2 panel varyanti deneniyor");
  Serial.println("   EKRANA BAK: 'N2' gorunuyor mu");
  d2.setI2CAddress(OLED_ADDR << 1);
  d2.begin();
  d2.setBusClock(100000);
  for (int i = 0; i < 3; i++) {
    bigText(d2, "N2", "NONAME2");
    reportBus("NONAME2 yazildi");
    delay(1500);
  }

  // ---- ADIM 7: 400 kHz ----
  step(7, "400 kHz hiz testi");
  d0.setBusClock(400000);
  for (int i = 0; i < 5; i++) {
    bigText(d0, "400", "kHz");
    reportBus("400 kHz");
    delay(800);
  }

  // ---- ADIM 8: 20 sayfa seri yazim (kilitlenme tekrar ediyor mu) ----
  step(8, "art arda 20 ekran — kilitlenme tekrar eder mi");
  int okPages = 0;
  for (int i = 1; i <= 20; i++) {
    char b[8];
    snprintf(b, sizeof(b), "%d/20", i);
    d0.clearBuffer();
    d0.setFont(u8g2_font_helvB12_tf);
    d0.drawStr(30, 40, b);
    d0.sendBuffer();
    bool ok = busAck();
    if (ok) okPages++;
    if (i % 5 == 0) Serial.printf("   %2d/20  bus=%s\n", i, ok ? "ACK" : "NAK");
    delay(220);
  }
  Serial.printf("   sonuc: %d/20 sayfa ACK\n", okPages);

  // ---- OZET ----
  Serial.println("\n==================================================");
  Serial.println("  OZET");
  Serial.println("==================================================");
  Serial.printf("  bus hata sayisi : %d\n", busFails);
  Serial.printf("  ACK sayfa       : %d/20\n", okPages);
  Serial.println();
  Serial.println("  YORUM:");
  if (busFails == 0 && okPages == 20) {
    Serial.println("   I2C hatti saglam. Sorun cizim/font tarafinda:");
    Serial.println("   hangi adimda ekran degistiyse onu kullan.");
  } else if (busFails > 0 && okPages < 20) {
    Serial.println("   I2C KILITLENIYOR. Sirasiyla dene:");
    Serial.println("   1) SCL/SDA kablolarini kisalt (15 cm alti)");
    Serial.println("   2) 100 kHz'de kal (400'u kullanma)");
    Serial.println("   3) SDA/SCL'ye harici 4.7k pull-up ekle (3V3'a)");
    Serial.println("   4) Kablo demetini 12V/guc kablosundan uzaklastir");
  } else {
    Serial.println("   Karisik: bazi yazimlar ulasiyor. Kablo temasi supheli.");
    Serial.println("   Tum kablo uclarini ve lehimleri kontrol et.");
  }
  Serial.println("\n  Test bitti. Tekrar icin EN tusuna bas.");
}

void loop() {
  // ekranda canli sayac: ekran HALA guncelleniyor mu?
  static unsigned long last = 0;
  if (millis() - last > 1000) {
    last = millis();
    char b[16];
    snprintf(b, sizeof(b), "%lus", millis() / 1000);
    d0.clearBuffer();
    d0.setFont(u8g2_font_helvB12_tf);
    d0.drawStr(20, 30, b);
    d0.setFont(u8g2_font_6x10_tf);
    d0.drawStr(20, 48, busAck() ? "bus OK" : "bus FAIL");
    d0.sendBuffer();
  }
}
