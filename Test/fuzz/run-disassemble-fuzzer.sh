#!/bin/bash

set -v

BUILD_DIR=$(pwd)
WAVM_DIR=$(cd `dirname $0`/../.. && pwd)

cd $BUILD_DIR

ninja

mkdir disassemble-corpus

ASAN_OPTIONS=detect_leaks=0 bin/FuzzDisassemble -use_value_profile=1 \
  -workers=18 \
  -jobs=18 \
  -detect_leaks=0 \
  disassemble-corpus \
    wasm-corpus \
    wasm-seed-corpus
