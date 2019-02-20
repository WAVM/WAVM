#!/bin/bash

set -v

BUILD_DIR=$(pwd)
WAVM_DIR=$(cd `dirname $0`/../.. && pwd)

cd $BUILD_DIR

ninja

mkdir compile-corpus

bin/FuzzCompile -use_value_profile=1 \
  -workers=36 \
  -jobs=36 \
  -rss_limit_mb=2048 \
  compile-corpus \
  	wasm-corpus \
	wasm-seed-corpus
