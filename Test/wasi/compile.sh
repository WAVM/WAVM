#!/bin/bash

set -e

WASI_SDK_DIR=$1
TEST_CPP=$2
TEST_DIR=$(dirname $TEST_CPP)
TEST_NAME=$(basename $TEST_CPP .cpp)

$WASI_SDK_DIR/opt/wasi-sdk/bin/clang \
  -O3 -g3 \
  -fdebug-prefix-map=$TEST_DIR=/ \
  -fdebug-prefix-map=$WASI_SDK_DIR/opt/wasi-sdk=/wasi-sdk \
  --sysroot $WASI_SDK_DIR/opt/wasi-sdk/share/wasi-sysroot/ \
  -o $TEST_DIR/$TEST_NAME.wasm \
  $TEST_CPP