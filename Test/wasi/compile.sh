#!/bin/bash

set -e

WASI_SDK_DIR=$1
TEST_CPP=$2
TEST_DIR=$(dirname $TEST_CPP)
TEST_NAME=$(basename $TEST_CPP .cpp)

$WASI_SDK_DIR/opt/wasi-sdk/bin/clang \
  -O3 -g3 \
  --sysroot $WASI_SDK_DIR/opt/wasi-sdk/share/sysroot/ \
  -o $TEST_DIR/$TEST_NAME.wasm \
  $TEST_CPP