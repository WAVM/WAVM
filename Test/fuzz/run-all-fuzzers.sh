#!/bin/bash

set -e

BUILD_DIR=$(pwd)
WAVM_DIR=$(cd `dirname $0`/../.. && pwd)
SCRIPT_DIR=$WAVM_DIR/Test/fuzz

WAST_DICTIONARY=$SCRIPT_DIR/wastFuzzDictionary.txt

WORKERS_PER_FUZZER=$(nproc --all)
JOBS_PER_FUZZER=$WORKERS_PER_FUZZER
SECONDS_PER_JOB=14400

# Build LLVM and WAVM.
ninja -C llvm/build
ninja

# Generate/update the seed corpus
$SCRIPT_DIR/generate-seed-corpus.sh

# Run the assemble fuzzer.
$SCRIPT_DIR/run-fuzzer-and-reduce-corpus.sh assemble \
	wast-seed-corpus \
	-jobs=$JOBS_PER_FUZZER \
	-workers=$WORKERS_PER_FUZZER \
	-max_total_time=$SECONDS_PER_JOB \
	-dict=$WAST_DICTIONARY

# Run the disassemble fuzzer.
$SCRIPT_DIR/run-fuzzer-and-reduce-corpus.sh disassemble \
	wasm-seed-corpus \
	-jobs=$JOBS_PER_FUZZER \
	-workers=$WORKERS_PER_FUZZER \
	-max_total_time=$SECONDS_PER_JOB

# Run the instantiate fuzzer.
$SCRIPT_DIR/run-fuzzer-and-reduce-corpus.sh instantiate \
	wasm-seed-corpus \
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