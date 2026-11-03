#include "btn.h"
#include "pins.h"

// ============================================================
//  Buton durum makinesi
//
//  ONEMLI: btns[] dizisi 0..3 indeksli, ama Btn enum'u BTN_NONE=0'dan
//  basliyor (BTN_UP=1, BTN_DOWN=2, BTN_BACK=3, BTN_OK=4). Bu yuzden
//  (Btn)i donusumu YANLIS: her buton bir otekine kayar ve BTN_OK hic
//  uretilmez. Acik esleme tablosu (kBtnOf) sart.
// ============================================================

struct BtnState {
  uint8_t  pin;
  bool     stable;        // filtrelenmis durum (true = basili)
  bool     last;          // son ham okuma
  uint32_t changedAt;
  uint32_t downAt;
  bool     longFired;
  uint32_t lastRepeat;
};

static BtnState btns[4] = {
  {PIN_BTN_UP,   false, false, 0, 0, false, 0},
  {PIN_BTN_DOWN, false, false, 0, 0, false, 0},
  {PIN_BTN_BACK, false, false, 0, 0, false, 0},
  {PIN_BTN_OK,   false, false, 0, 0, false, 0},
};

// dizi indeksi -> Btn
static const Btn kBtnOf[4] = { BTN_UP, BTN_DOWN, BTN_BACK, BTN_OK };

static const uint32_t DEBOUNCE_MS = 25;
static const uint32_t LONG_MS     = 800;
static const uint32_t REPEAT_MS   = 140;

int btnPin(Btn b) {
  switch (b) {
    case BTN_UP:   return PIN_BTN_UP;
    case BTN_DOWN: return PIN_BTN_DOWN;
    case BTN_BACK: return PIN_BTN_BACK;
    case BTN_OK:   return PIN_BTN_OK;
    default:       return -1;
  }
}

// basili mi? (BTN_ACTIVE_LOW'a gore)
static inline bool pressed(int pin) {
#if BTN_ACTIVE_LOW
  return digitalRead(pin) == LOW;
#else
  return digitalRead(pin) == HIGH;
#endif
}

int btnPressedPin() {
  for (int i = 0; i < 4; i++) {
    if (pressed(btns[i].pin)) return btns[i].pin;
  }
  return -1;
}

void btnBegin() {
  // oz-denetim: esleme tablosu pin dizisiyle tutarli mi?
  for (int i = 0; i < 4; i++) {
    int want = btnPin(kBtnOf[i]);
    if (want != (int)btns[i].pin) {
      Serial.printf("[buton] ESLEME HATASI: indeks %d pin %d, beklenen %d\n",
                    i, btns[i].pin, want);
    }
  }
  Serial.printf("[buton] EVET=%d HAYIR=%d GERI=%d OK=%d\n",
                btnPin(BTN_UP), btnPin(BTN_DOWN),
                btnPin(BTN_BACK), btnPin(BTN_OK));

  for (int i = 0; i < 4; i++) {
#if BTN_ACTIVE_LOW
    pinMode(btns[i].pin, INPUT_PULLUP);
#else
    pinMode(btns[i].pin, INPUT_PULLDOWN);
#endif
    btns[i].stable = btns[i].last = pressed(btns[i].pin);
    btns[i].changedAt = millis();
    if (btns[i].stable) {
      Serial.printf("[buton] DIKKAT: GPIO %d acilista BASILI gorunuyor\n",
                    btns[i].pin);
    }
  }
}

BtnEvent btnPoll(Btn* which) {
  uint32_t now = millis();

  for (int i = 0; i < 4; i++) {
    BtnState& b = btns[i];
    bool raw = pressed(b.pin);

    if (raw != b.last) { b.last = raw; b.changedAt = now; }

    if (raw != b.stable && (now - b.changedAt) >= DEBOUNCE_MS) {
      b.stable = raw;

      if (b.stable) {                    // basildi
        b.downAt     = now;
        b.longFired  = false;
        b.lastRepeat = now;
      } else {                           // birakildi
        if (!b.longFired) {
          *which = kBtnOf[i];
          return EV_CLICK;
        }
      }
    }

    if (b.stable) {
      if (!b.longFired && (now - b.downAt) >= LONG_MS) {
        b.longFired  = true;
        b.lastRepeat = now;
        *which = kBtnOf[i];
        return EV_LONG;
      }
      if (b.longFired && (now - b.lastRepeat) >= REPEAT_MS) {
        b.lastRepeat = now;
        *which = kBtnOf[i];
        return EV_REPEAT;
      }
    }
  }
  return EV_NONE;
}

bool btnIsDown(Btn b) {
  for (int i = 0; i < 4; i++) {
    if (kBtnOf[i] == b) return btns[i].stable;
  }
  return false;
}
