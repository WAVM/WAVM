#!/bin/bash

set -v

BUILD_DIR=$(pwd)
WAVM_DIR=$(cd `dirname $0`/../.. && pwd)

cd $BUILD_DIR

ninja

mkdir assemble-corpus

bin/FuzzAssemble -use_value_profile=1 \
  -workers=36 \
  -jobs=36 \
  -dict=$WAVM_DIR/Test/fuzz/wastFuzzDictionary.txt \
  assemble-corpus \
    wast-corpus \
    wast-seed-corpus
