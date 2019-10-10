#!/bin/bash

set -e

BUILD_DIR=$(pwd)
WAVM_DIR=$(cd `dirname $0`/../.. && pwd)
SCRIPT_DIR=$WAVM_DIR/Test/fuzz

WAST_DICTIONARY=$SCRIPT_DIR/wastFuzzDictionary.txt

WORKERS_PER_FUZZER=$(nproc --all)
JOBS_PER_FUZZER=$WORKERS_PER_FUZZER
SECONDS_PER_JOB=3600

mkdir -p wast-seed-corpus

$SCRIPT_DIR/run-fuzzer-and-reduce-corpus.sh assemble \
	wast-seed-corpus \
	-jobs=$JOBS_PER_FUZZER \
	-workers=$WORKERS_PER_FUZZER \
	-max_total_time=$SECONDS_PER_JOB \
	-max_len=50000 \
	-dict=$WAST_DICTIONARY
