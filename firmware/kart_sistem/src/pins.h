#pragma once
#include <Arduino.h>
#include "board.h"

// ============================================================ pinler
// Buton ve modul pinleri board.h'den gelir — orada degistir.
#define PIN_SDA       OLED_SDA_PIN
#define PIN_SCL       OLED_SCL_PIN

#define PIN_RFID_CS   RFID_CS_PIN
#define PIN_RFID_RST  RFID_RST_PIN
#define PIN_SCK       RFID_SCK_PIN
#define PIN_MOSI      RFID_MOSI_PIN
#define PIN_MISO      RFID_MISO_PIN

#define PIN_BTN_UP    BTN_UP_PIN
#define PIN_BTN_DOWN  BTN_DOWN_PIN
#define PIN_BTN_BACK  BTN_BACK_PIN
#define PIN_BTN_OK    BTN_OK_PIN

// ---- derleme zamani denetim: ayni pine iki sey baglanamaz ----
#if BTN_UP_PIN == BTN_DOWN_PIN || BTN_UP_PIN == BTN_BACK_PIN || \
    BTN_UP_PIN == BTN_OK_PIN   || BTN_DOWN_PIN == BTN_BACK_PIN || \
    BTN_DOWN_PIN == BTN_OK_PIN || BTN_BACK_PIN == BTN_OK_PIN
#error "board.h: iki buton ayni GPIO'ya atanmis"
#endif

#if BTN_OK_PIN == OLED_SDA_PIN || BTN_OK_PIN == OLED_SCL_PIN || \
    BTN_BACK_PIN == OLED_SDA_PIN || BTN_BACK_PIN == OLED_SCL_PIN || \
    BTN_UP_PIN == OLED_SDA_PIN || BTN_UP_PIN == OLED_SCL_PIN || \
    BTN_DOWN_PIN == OLED_SDA_PIN || BTN_DOWN_PIN == OLED_SCL_PIN
#error "board.h: buton OLED I2C hattiyla cakisiyor (SDA=21 / SCL=22)"
#endif

#if BTN_OK_PIN == RFID_CS_PIN || BTN_OK_PIN == RFID_RST_PIN || \
    BTN_OK_PIN == RFID_SCK_PIN || BTN_OK_PIN == RFID_MOSI_PIN || \
    BTN_OK_PIN == RFID_MISO_PIN
#error "board.h: buton RC522 hattiyla cakisiyor"
#endif

#define FW_VERSION    "0.2.0"
// Ekranda gorunen cihaz adi. Uretimde -D DEVICE_NAME='"EVCHARGE-01"' ile degistir.
#ifndef DEVICE_NAME
#define DEVICE_NAME   "EVCHARGE-01"
#endif
