#!/bin/bash

set -e

BUILD_DIR=$(pwd)
WAVM_DIR=$(cd `dirname $0`/../.. && pwd)
SCRIPT_DIR=$WAVM_DIR/Test/fuzz

WAST_DICTIONARY=$SCRIPT_DIR/wastFuzzDictionary.txt

SECONDS_PER_JOB=3600

mkdir -p wast-seed-corpus

$SCRIPT_DIR/run-fuzzer-and-reduce-corpus.sh assemble \
	wast-seed-corpus \
	-max_total_time=$SECONDS_PER_JOB \
	-max_len=50000 \
	-dict=$WAST_DICTIONARY
