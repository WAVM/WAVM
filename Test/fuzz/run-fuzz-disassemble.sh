#!/bin/bash

set -e

BUILD_DIR=$(pwd)
WAVM_DIR=$(cd `dirname $0`/../.. && pwd)
SCRIPT_DIR=$WAVM_DIR/Test/fuzz

WORKERS_PER_FUZZER=$(nproc --all)
JOBS_PER_FUZZER=$WORKERS_PER_FUZZER
SECONDS_PER_JOB=3600

mkdir -p wasm-seed-corpus
mkdir -p translated-compile-model-corpus

$SCRIPT_DIR/run-fuzzer-and-reduce-corpus.sh disassemble \
	wasm-seed-corpus \
	translated-compile-model-corpus \
	-jobs=$JOBS_PER_FUZZER \
	-workers=$WORKERS_PER_FUZZER \
	-max_total_time=$SECONDS_PER_JOB \
	-max_len=5000
