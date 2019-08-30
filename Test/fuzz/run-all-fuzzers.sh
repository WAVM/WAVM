#!/bin/bash

set -e

BUILD_DIR=$(pwd)
WAVM_DIR=$(cd `dirname $0`/../.. && pwd)
SCRIPT_DIR=$WAVM_DIR/Test/fuzz

# Build LLVM and WAVM.
ninja -C llvm/build
ninja

# Generate/update the seed corpus
$SCRIPT_DIR/generate-seed-corpus.sh

# Run the assemble fuzzer.
$SCRIPT_DIR/run-fuzz-assemble.sh

# Run the disassemble fuzzer.
$SCRIPT_DIR/run-fuzz-disassemble.sh

# Run the compile model fuzzer.
$SCRIPT_DIR/run-fuzz-compile-model.sh

# Translate the compile model corpus to WASM files
mkdir -p corpora/compile-model
rm -rf translated-compile-model-corpus
mkdir -p translated-compile-model-corpus
bin/translate-compile-model-corpus \
	corpora/compile-model \
	translated-compile-model-corpus

# Run the instantiate fuzzer.
$SCRIPT_DIR/run-fuzz-instantiate.sh
