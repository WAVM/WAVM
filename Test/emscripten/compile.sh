#!/bin/bash

set -e

TEST_CPP=$1
TEST_DIR=$(dirname $TEST_CPP)
TEST_NAME=$(basename $TEST_CPP .cpp)

emcc $TEST_CPP \
  -O3 -g3 \
  -std=c++11 \
  -s STANDALONE_WASM=1 \
  ${@:2} \
  -o $TEST_DIR/$TEST_NAME.wasm

rm -f $TEST_DIR/$TEST_NAME.wasm.map
rm -f $TEST_DIR/$TEST_NAME.wast
rm -f $TEST_DIR/$TEST_NAME.js
rm -f $TEST_DIR/$TEST_NAME.worker.js
