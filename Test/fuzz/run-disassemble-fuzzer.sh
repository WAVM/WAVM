#!/bin/bash

set -v

BUILD_DIR=$(pwd)
WAVM_DIR=$(cd `dirname $0`/../.. && pwd)

cd $BUILD_DIR

ninja

mkdir disassemble-corpus

bin/FuzzDisassemble -use_value_profile=1 \
  -workers=36 \
  -jobs=36 \
  disassemble-corpus \
    wasm-corpus \
    wasm-seed-corpus
