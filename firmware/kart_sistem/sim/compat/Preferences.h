#pragma once
// Host shim — NVS'yi bellek ici taklit eder (testler icin gercekci).
#include "Arduino.h"
#include <map>
#include <vector>

struct PrefStore {
  std::map<std::string, std::vector<uint8_t>> bytes;
  std::map<std::string, int> ints;
  int puts = 0;                 // yazim sayaci (flash omru testi icin)
};
extern PrefStore gPrefs;

class Preferences {
 public:
  bool begin(const char*, bool) { return true; }

  size_t getBytesLength(const char* k) {
    auto it = gPrefs.bytes.find(k);
    return it == gPrefs.bytes.end() ? 0 : it->second.size();
  }
  size_t getBytes(const char* k, void* out, size_t len) {
    auto it = gPrefs.bytes.find(k);
    if (it == gPrefs.bytes.end()) return 0;
    size_t n = it->second.size() < len ? it->second.size() : len;
    memcpy(out, it->second.data(), n);
    return n;
  }
  size_t putBytes(const char* k, const void* in, size_t len) {
    gPrefs.bytes[k] = std::vector<uint8_t>((const uint8_t*)in, (const uint8_t*)in + len);
    gPrefs.puts++;
    return len;
  }
  int getInt(const char* k, int d = 0) {
    auto it = gPrefs.ints.find(k);
    return it == gPrefs.ints.end() ? d : it->second;
  }
  size_t putInt(const char* k, int v) { gPrefs.ints[k] = v; gPrefs.puts++; return sizeof(int); }

  uint32_t getUInt(const char* k, uint32_t d = 0) {
    auto it = gPrefs.ints.find(k);
    return it == gPrefs.ints.end() ? d : (uint32_t)it->second;
  }
  size_t putUInt(const char* k, uint32_t v) { gPrefs.ints[k] = (int)v; gPrefs.puts++; return sizeof(uint32_t); }
  bool remove(const char*) { return true; }
};
