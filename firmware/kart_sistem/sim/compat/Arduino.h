#pragma once
// Minimal Arduino shim — ui.cpp / hw.cpp'yi Mac'te derlemek icin.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <cmath>
#include <algorithm>

typedef uint8_t byte;

class String : public std::string {
 public:
  String() {}
  String(const char* s) : std::string(s ? s : "") {}
  String(const std::string& s) : std::string(s) {}
  const char* c_str() const { return std::string::c_str(); }
  String& operator+=(const String& o) { std::string::operator+=(o); return *this; }
};

// ---- sahte zaman: testler millis()'i ilerletebilir ----
extern uint32_t gFakeMillis;
inline uint32_t millis() { return gFakeMillis; }
inline void delay(uint32_t) {}

// ---- sahte GPIO: testler pin durumunu belirler ----
#define INPUT          0
#define OUTPUT         1
#define INPUT_PULLUP   2
#define INPUT_PULLDOWN 3
#define LOW            0
#define HIGH           1

extern uint8_t gPinState[64];      // 0 = LOW, 1 = HIGH
extern uint8_t gPinMode[64];

inline void    pinMode(uint8_t pin, uint8_t mode) { gPinMode[pin] = mode; }
inline int     digitalRead(uint8_t pin) { return gPinState[pin]; }
inline void    digitalWrite(uint8_t pin, uint8_t v) { gPinState[pin] = v; }

template <typename T> T constrain(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

#define PROGMEM
#define F(x) (x)

struct SerialShim {
  void begin(int) {}
  void print(const char*) {}
  void println(const char*) {}
  void println() {}
  int printf(const char*, ...) { return 0; }
};
extern SerialShim Serial;
