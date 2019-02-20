#!/bin/bash

set -v

BUILD_DIR=$(pwd)
WAVM_DIR=$(cd `dirname $0`/../.. && pwd)

cd $BUILD_DIR

ninja

mkdir instantiate-corpus

bin/FuzzInstantiate -use_value_profile=1 \
  -workers=18 \
  -jobs=18 \
  -rss_limit_mb=4096 \
  instantiate-corpus \
	compile-corpus \
	wasm-corpus \
	wasm-seed-corpus
