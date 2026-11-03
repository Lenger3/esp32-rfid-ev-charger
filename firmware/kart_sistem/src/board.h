#pragma once

// ===========================================================================
//  BUTON ESLEMESI
//
//  Fiziksel butonlar karisik baglandiysa SADECE bu sayilari degistir.
//  Hangi butonun hangi GPIO'ya gittigini bilmiyorsan:
//    Yonetici menusu -> "Buton Testi" -> her butona tek tek bas.
//    Ekran bastigin GPIO'yu gosterir. Sonra buraya yaz.
//
//  Semaya gore "UP" = yukari giden buton (menude yukari), GPIO degil.
// ===========================================================================

#define BTN_UP_PIN     26
#define BTN_DOWN_PIN   33
#define BTN_BACK_PIN   32
#define BTN_OK_PIN     25

// 1 = buton basinca GPIO'yu GND'ye cekiyor (en yaygin, INPUT_PULLUP)
// 0 = buton basinca GPIO'yu 3V3'e cekiyor (INPUT_PULLDOWN)
#define BTN_ACTIVE_LOW 1

// ===========================================================================
//  OLED / RC522 — dogrulanmis, degistirmeye gerek yok
// ===========================================================================
// --- sarj cihazi surucusu ---
// 1 = simulasyon (sahte kWh, cihaz tetiklenmez)  |  0 = gercek Tuya LAN
#define CHARGER_SIM     0

#define OLED_SDA_PIN   22
#define OLED_SCL_PIN   21

#define RFID_CS_PIN     5
#define RFID_RST_PIN   27
#define RFID_SCK_PIN   18
#define RFID_MOSI_PIN  23
#define RFID_MISO_PIN  19
