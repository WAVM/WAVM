#!/bin/bash

set -v

BUILD_DIR=$(pwd)
WAVM_DIR=$(cd `dirname $0`/../.. && pwd)

cd $BUILD_DIR

ninja

mkdir wasm-corpus

bin/FuzzWASM -use_value_profile=1 \
  -workers=36 \
  -jobs=36 \
  wasm-corpus \
	wasm-seed-corpus
