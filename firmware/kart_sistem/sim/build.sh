#!/bin/bash
# Firmware'in gercek ui.cpp dosyasini Mac'te derler ve ekran goruntusu uretir.
set -e
cd "$(dirname "$0")"
LIB=../.pio/libdeps/esp32dev/U8g2/src
OBJ=obj
mkdir -p $OBJ out

CFLAGS="-O1 -w -I$LIB -I$LIB/clib"
CXXFLAGS="-std=c++17 -O1 -w -DHOST_SIM -Icompat -I../src -I$LIB -I$LIB/clib"

OBJS=""
for src in "$LIB"/clib/*.c; do
  f=$(basename "$src" .c)
  o=$OBJ/$f.o
  if [ ! -f "$o" ] || [ "$src" -nt "$o" ]; then
    gcc $CFLAGS -c "$src" -o "$o"
  fi
  OBJS="$OBJS $o"
done

g++ $CXXFLAGS -c ../src/ui.cpp -o $OBJ/ui.o
g++ $CXXFLAGS -c sim_main.cpp -o $OBJ/sim_main.o

U8g2lib=""
if [ ! -f $OBJ/u8g2libcpp.o ]; then
  g++ -std=c++17 -O1 -w -I$LIB -I$LIB/clib -c "$LIB/U8g2lib.cpp" -o $OBJ/u8g2libcpp.o
fi

g++ $OBJS $OBJ/ui.o $OBJ/sim_main.o $OBJ/u8g2libcpp.o -o sim
./sim
