#pragma once
#include <Arduino.h>

// ============================================================
//  Butonlar — hw.cpp'den ayri, cunku sadece digitalRead kullaniyor
//  ve Mac'te birim testi yapilabiliyor (sim/test_buttons.cpp).
// ============================================================

enum Btn { BTN_NONE = 0, BTN_UP, BTN_DOWN, BTN_BACK, BTN_OK };
enum BtnEvent { EV_NONE = 0, EV_CLICK, EV_LONG, EV_REPEAT };

void     btnBegin();                 // pinleri INPUT_PULLUP yapar
BtnEvent btnPoll(Btn* which);        // her loop'ta cagir
bool     btnIsDown(Btn b);
int      btnPin(Btn b);              // board.h'deki GPIO
int      btnPressedPin();            // o an basili GPIO, yoksa -1
