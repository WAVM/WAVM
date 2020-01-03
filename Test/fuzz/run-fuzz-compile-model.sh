#!/bin/bash

set -e

BUILD_DIR=$(pwd)
WAVM_DIR=$(cd `dirname $0`/../.. && pwd)
SCRIPT_DIR=$WAVM_DIR/Test/fuzz

SECONDS_PER_JOB=43200 # 12 hours

$SCRIPT_DIR/run-fuzzer-and-reduce-corpus.sh compile-model \
	-max_total_time=$SECONDS_PER_JOB \
	-max_len=20 \
	-reduce_inputs=0
