// ===========================================================================
//  BUTON MAKINESI TESTI — gercek btn.cpp uzerinde
//
//  Yakalanan hata: btns[] dizisi 0..3 indeksli, Btn enum'u BTN_NONE=0'dan
//  basliyor. (Btn)i donusumu her butonu BIR OTEKINE kaydiriyordu:
//    EVET  -> BTN_NONE  (olu)
//    HAYIR -> BTN_UP
//    GERI  -> BTN_DOWN
//    OK    -> BTN_BACK  (menuden cikiyordu)
//
//  Bu test o hatayi bir daha kacirmamak icin var.
// ===========================================================================
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "Arduino.h"
#include "../src/btn.h"
#include "../src/board.h"

SerialShim Serial;
uint32_t   gFakeMillis = 1000;
uint8_t    gPinState[64];
uint8_t    gPinMode[64];

static int gPass = 0, gFail = 0;

#define CHECK(cond, msg) do { \
  if (cond) { gPass++; printf("  [OK  ] %s\n", msg); } \
  else      { gFail++; printf("  [FAIL] %s\n", msg); } \
} while (0)

static const char* nm(Btn b) {
  switch (b) {
    case BTN_UP:   return "EVET";
    case BTN_DOWN: return "HAYIR";
    case BTN_BACK: return "GERI";
    case BTN_OK:   return "OK";
    default:       return "NONE";
  }
}

struct Ev { BtnEvent ev = EV_NONE; Btn who = BTN_NONE; };

// zamani ilerlet ve btnPoll'u tekrar tekrar cagir (gercek loop gibi).
// Ilk uretilen olayi dondurur.
static Ev pump(uint32_t ms, uint32_t step = 10) {
  Ev r;
  for (uint32_t t = 0; t < ms; t += step) {
    gFakeMillis += step;
    Btn w = BTN_NONE;
    BtnEvent e = btnPoll(&w);
    if (e != EV_NONE && r.ev == EV_NONE) { r.ev = e; r.who = w; }
  }
  return r;
}

// butonu holdMs basili tut, birak; olayi dondur
static Ev tap(int pin, uint32_t holdMs = 120) {
  gPinState[pin] = LOW;
  pump(60);                       // debounce: basildi
  Ev hold = pump(holdMs);         // basili tut
  gPinState[pin] = HIGH;
  Ev up = pump(80);               // debounce: birakildi -> CLICK

  if (up.ev != EV_NONE)   return up;
  if (hold.ev != EV_NONE) return hold;
  return Ev{};
}

int main() {
  printf("=== BUTON MAKINESI TESTI (gercek btn.cpp) ===\n");

  memset(gPinState, HIGH, sizeof(gPinState));   // hepsi bosta
  btnBegin();

  printf("\n--- 1. pin haritasi (board.h) ---\n");
  printf("  EVET=%d HAYIR=%d GERI=%d OK=%d\n",
         btnPin(BTN_UP), btnPin(BTN_DOWN), btnPin(BTN_BACK), btnPin(BTN_OK));
  CHECK(btnPin(BTN_UP)   == BTN_UP_PIN,   "EVET  -> BTN_UP_PIN");
  CHECK(btnPin(BTN_DOWN) == BTN_DOWN_PIN, "HAYIR -> BTN_DOWN_PIN");
  CHECK(btnPin(BTN_BACK) == BTN_BACK_PIN, "GERI  -> BTN_BACK_PIN");
  CHECK(btnPin(BTN_OK)   == BTN_OK_PIN,   "OK    -> BTN_OK_PIN");

  CHECK(BTN_UP_PIN != BTN_DOWN_PIN && BTN_UP_PIN != BTN_BACK_PIN &&
        BTN_UP_PIN != BTN_OK_PIN && BTN_DOWN_PIN != BTN_BACK_PIN &&
        BTN_DOWN_PIN != BTN_OK_PIN && BTN_BACK_PIN != BTN_OK_PIN,
        "dort pin birbirinden farkli");

  printf("\n--- 2. her buton DOGRU Btn uretiyor mu ---\n");
  struct Case { int pin; Btn want; const char* lbl; };
  Case cases[4] = {
    {BTN_UP_PIN,   BTN_UP,   "EVET  (GPIO 26)"},
    {BTN_DOWN_PIN, BTN_DOWN, "HAYIR (GPIO 33)"},
    {BTN_BACK_PIN, BTN_BACK, "GERI  (GPIO 32)"},
    {BTN_OK_PIN,   BTN_OK,   "OK    (GPIO 25)"},
  };

  for (int i = 0; i < 4; i++) {
    Ev r = tap(cases[i].pin);
    char msg[128];
    snprintf(msg, sizeof(msg), "%s -> %s  (beklenen %s)",
             cases[i].lbl, nm(r.who), nm(cases[i].want));
    CHECK(r.ev == EV_CLICK && r.who == cases[i].want, msg);
  }

  printf("\n--- 3. kayma yok ---\n");
  bool shifted = false;
  for (int i = 0; i < 4; i++) {
    Ev r = tap(cases[i].pin);
    if (r.who != cases[i].want) shifted = true;
  }
  CHECK(!shifted, "hicbir tus bir otekine kaymadi");

  printf("\n--- 4. BTN_OK gercekten ulasilabiliyor ---\n");
  Ev okr = tap(BTN_OK_PIN);
  CHECK(okr.ev == EV_CLICK && okr.who == BTN_OK,
        "OK tusu BTN_OK donduruyor (eski kodda BTN_BACK donuyordu)");

  printf("\n--- 5. kisa temas filtreleniyor (debounce) ---\n");
  gPinState[BTN_UP_PIN] = LOW;
  pump(3);                             // 3 ms: cok kisa
  gPinState[BTN_UP_PIN] = HIGH;
  Ev glitch = pump(120);
  CHECK(glitch.ev == EV_NONE, "3 ms'lik glitch olay uretmiyor");

  printf("\n--- 6. basili tutma -> EV_LONG ---\n");
  gPinState[BTN_OK_PIN] = LOW;
  pump(60);                            // debounce: basildi
  Ev mid = pump(400, 20);
  CHECK(mid.ev == EV_NONE, "400 ms'de henuz EV_LONG yok");
  Ev lng = pump(500, 20);
  CHECK(lng.ev == EV_LONG, "900 ms basili tutma EV_LONG uretiyor");
  CHECK(lng.who == BTN_OK, "EV_LONG dogru tusu tasiyor (OK)");
  gPinState[BTN_OK_PIN] = HIGH;
  pump(80);

  printf("\n--- 7. btnIsDown ---\n");
  pump(120);
  CHECK(!btnIsDown(BTN_OK), "bosta BTN_OK basili degil");

  gPinState[BTN_OK_PIN] = LOW;
  pump(60);
  CHECK(btnIsDown(BTN_OK), "basinca BTN_OK basili goruluyor");
  CHECK(!btnIsDown(BTN_UP), "baska tus basili gorunmuyor");

  gPinState[BTN_OK_PIN] = HIGH;
  pump(80);
  CHECK(!btnIsDown(BTN_OK), "birakinca tekrar bos");

  printf("\n--- 8. btnPressedPin (buton testi ekrani icin) ---\n");
  gPinState[BTN_BACK_PIN] = LOW;
  pump(60);
  CHECK(btnPressedPin() == BTN_BACK_PIN, "basili GPIO dogru (32)");
  gPinState[BTN_BACK_PIN] = HIGH;
  pump(80);
  CHECK(btnPressedPin() == -1, "bos birakinca -1");

  printf("\n=== SONUC: %d gecti, %d kaldi ===\n", gPass, gFail);
  return gFail == 0 ? 0 : 1;
}
