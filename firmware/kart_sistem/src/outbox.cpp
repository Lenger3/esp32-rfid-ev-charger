#include "outbox.h"
#include <Preferences.h>

static Preferences p;
static OutRec  q[OUTBOX_MAX];
static int     n = 0;
static uint32_t seq = 0;

void outboxBegin() {
  p.begin("outbox", false);
  seq = p.getUInt("seq", 1);
  n = p.getInt("n", 0);
  if (n < 0 || n > OUTBOX_MAX) n = 0;

  size_t want = sizeof(q[0]) * n;
  if (n && p.getBytesLength("q") >= want) {
    p.getBytes("q", q, want);
  } else {
    n = 0;
  }
  Serial.printf("[outbox] kuyruk=%d seq=%u\n", n, seq);
}

void outboxSave() {
  p.putBytes("q", q, sizeof(q[0]) * n);
  p.putInt("n", n);
  p.putUInt("seq", seq);
}

int outboxCount() { return n; }

OutRec* outboxAt(int i) {
  if (i < 0 || i >= n) return nullptr;
  return &q[i];
}

uint32_t outboxNextId() { return seq++; }

bool outboxPush(const OutRec& r) {
  // doluysa en eskiyi at (en yeni veri daha degerli)
  if (n >= OUTBOX_MAX) {
    for (int i = 1; i < n; i++) q[i - 1] = q[i];
    n = OUTBOX_MAX - 1;
    Serial.println("[outbox] dolu, en eski kayit atildi");
  }
  q[n++] = r;
  outboxSave();
  return true;
}

void outboxDrop(int i) {
  if (i < 0 || i >= n) return;
  for (int k = i; k < n - 1; k++) q[k] = q[k + 1];
  n--;
  outboxSave();
}
