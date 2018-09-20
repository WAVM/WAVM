#!/bin/bash

set -v

BUILD_DIR=$(pwd)
WAVM_DIR=$(cd `dirname $0`/../.. && pwd)

cd $BUILD_DIR

ninja

mkdir assemble-corpus

ASAN_OPTIONS=detect_leaks=0 bin/FuzzAssemble -use_value_profile=1 \
  -workers=36 \
  -jobs=36 \
  -dict=$WAVM_DIR/Test/fuzz/wastFuzzDictionary.txt \
  -detect_leaks=0 \
  assemble-corpus \
    wast-corpus \
    wast-seed-corpus
