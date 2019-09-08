#!/bin/bash

# Usage:
#  run-fuzzer <fuzzer> <args...>

set -e

BUILD_DIR=$(pwd)
WAVM_DIR=$(cd `dirname $0`/../.. && pwd)
FUZZER=$1
EXTRA_ARGS=${@:2}

cd $BUILD_DIR

# Make sure the output corpus and artifact directories exist.
mkdir -p corpora/${FUZZER}
mkdir -p artifacts/${FUZZER}

# Run the fuzzer.
bin/fuzz-${FUZZER} \
	corpora/${FUZZER} \
	-artifact_prefix="artifacts/${FUZZER}/" \
	-use_value_profile=1 \
	-print_final_stats=1 \
	-print_pcs=1 \
	-reduce_depth=1 \
	-shrink=1 \
	$EXTRA_ARGS

# Copy the current corpus into the archive
cp -rf corpora/${FUZZER} corpora/archive
