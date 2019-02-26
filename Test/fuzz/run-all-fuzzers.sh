#!/bin/bash

set -e

BUILD_DIR=$(pwd)
WAVM_DIR=$(cd `dirname $0`/../.. && pwd)
SCRIPT_DIR=$WAVM_DIR/Test/fuzz

WAST_DICTIONARY=$SCRIPT_DIR/wastFuzzDictionary.txt

JOBS_PER_FUZZER=32
WORKERS_PER_FUZZER=32
SECONDS_PER_JOB=7200

# Pull the latest WAVM code
cd $WAVM_DIR
git pull
cd $BUILD_DIR

# Build LLVM and WAVM.
ninja -C llvm/build
ninja

# Generate/update the seed corpus
$SCRIPT_DIR/generate-seed-corpus.sh

# Run the WAST fuzzer.
$SCRIPT_DIR/run-fuzzer-and-reduce-corpus.sh wast \
	wast-seed-corpus \
	-jobs=$JOBS_PER_FUZZER \
	-workers=$WORKERS_PER_FUZZER \
	-max_total_time=$SECONDS_PER_JOB \
	-dict=$WAST_DICTIONARY

# Run the assemble fuzzer.
$SCRIPT_DIR/run-fuzzer-and-reduce-corpus.sh assemble \
	corpora/wast \
	-jobs=$JOBS_PER_FUZZER \
	-workers=$WORKERS_PER_FUZZER \
	-max_total_time=$SECONDS_PER_JOB \
	-dict=$WAST_DICTIONARY

# Run the WASM fuzzer.
$SCRIPT_DIR/run-fuzzer-and-reduce-corpus.sh wasm \
	wasm-seed-corpus \
	-jobs=$JOBS_PER_FUZZER \
	-workers=$WORKERS_PER_FUZZER \
	-max_total_time=$SECONDS_PER_JOB

# Run the disassemble fuzzer.
$SCRIPT_DIR/run-fuzzer-and-reduce-corpus.sh disassemble \
	corpora/wasm \
	-jobs=$JOBS_PER_FUZZER \
	-workers=$WORKERS_PER_FUZZER \
	-max_total_time=$SECONDS_PER_JOB

# Run the instantiate fuzzer.
$SCRIPT_DIR/run-fuzzer-and-reduce-corpus.sh instantiate \
	corpora/wasm \
	-jobs=$JOBS_PER_FUZZER \
	-workers=$WORKERS_PER_FUZZER \
	-max_total_time=$SECONDS_PER_JOB

# Run the compile model fuzzer.
$SCRIPT_DIR/run-fuzzer-and-reduce-corpus.sh compile-model \
	-jobs=$JOBS_PER_FUZZER \
	-workers=$WORKERS_PER_FUZZER \
	-max_total_time=$SECONDS_PER_JOB \
	-max_len=20 \
	-reduce_inputs=0