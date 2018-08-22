#!/bin/bash

set -v

BUILD_DIR=$(pwd)
WAVM_DIR=$(cd `dirname $0`/../.. && pwd)

cd $BUILD_DIR

ninja

mkdir runtime-corpus

ASAN_OPTIONS=detect_leaks=0 bin/FuzzRuntime -use_value_profile=1 \
  -workers=18 \
  -jobs=18 \
  -detect_leaks=0 \
  -rss_limit_mb=4096 \
  runtime-corpus \
  	wasm-corpus \
	wasm-seed-corpus
