#!/bin/bash

set -v

BUILD_DIR=$(pwd)
WAVM_DIR=$(cd `dirname $0`/../.. && pwd)

cd $BUILD_DIR

ninja

mkdir instantiate-corpus

ASAN_OPTIONS=detect_leaks=0 bin/FuzzInstantiate -use_value_profile=1 \
  -workers=36 \
  -jobs=36 \
  -detect_leaks=0 \
  -rss_limit_mb=2048 \
  instantiate-corpus \
	compile-corpus \
	wasm-corpus \
	wasm-seed-corpus
