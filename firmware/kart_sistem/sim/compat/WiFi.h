#pragma once
#include "Arduino.h"

#define WL_CONNECTED 3

class IPShim {
 public:
  String toString() const { return String("192.168.1.42"); }
};

class WiFiShim {
 public:
  IPShim localIP() { return IPShim(); }
  int status() { return WL_CONNECTED; }
  void mode(int) {}
  void begin(const char*, const char*) {}
};

extern WiFiShim WiFi;
