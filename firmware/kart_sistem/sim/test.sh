#!/bin/bash
# Mac'te calisan birim testleri — donanim gerekmez.
#   ./test.sh
set -e
cd "$(dirname "$0")"

CXX="g++ -std=c++17 -O1 -w -DHOST_SIM -Icompat -I../src"

echo "=============================================="
echo "  BIRIM TESTLERI"
echo "=============================================="

echo
echo ">>> buton durum makinesi (src/btn.cpp)"
$CXX test_buttons.cpp ../src/btn.cpp -o .test_buttons
./.test_buttons
B=$?

echo
echo ">>> kalici depolama (src/store.cpp)"
$CXX test_store.cpp ../src/store.cpp -o .test_store
./.test_store
S=$?

rm -f .test_buttons .test_store

echo
if [ $B -eq 0 ] && [ $S -eq 0 ]; then
  echo "TUM TESTLER GECTI"
  exit 0
else
  echo "BASARISIZ: buton=$B store=$S"
  exit 1
fi
