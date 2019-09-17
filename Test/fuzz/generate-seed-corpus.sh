#!/bin/bash

BUILD_DIR=$(pwd)
WAVM_DIR=$(cd `dirname $0`/../.. && pwd)

cd $BUILD_DIR

ninja

rm -rf wast-seed-corpus
rm -rf wasm-seed-corpus

mkdir -p wast-seed-corpus

find $WAVM_DIR/Test \
  -iname *.wast -not -iname skip-stack-guard-page.wast -not -iname br_table.wast \
  | xargs -n1 bin/wavm test dumpmodules --wast --output-dir wast-seed-corpus 

mkdir -p wasm-seed-corpus

find $WAVM_DIR/Test \
  -iname *.wast -not -iname skip-stack-guard-page.wast -not -iname br_table.wast \
  | xargs -n1 bin/wavm test dumpmodules --wasm --output-dir wasm-seed-corpus 
