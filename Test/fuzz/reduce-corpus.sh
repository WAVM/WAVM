#!/bin/bash

# Usage:
#  reduce-corpus <fuzzer> <args...>

set -e

BUILD_DIR=$(pwd)
WAVM_DIR=$(cd `dirname $0`/../.. && pwd)
FUZZER=$1
EXTRA_ARGS=${@:2}

cd $BUILD_DIR

# Make a new reduced corpus directory.
rm -rf corpora/${FUZZER}-reduced
mkdir -p corpora/${FUZZER}-reduced

# Run the fuzzer.
bin/fuzz-${FUZZER} \
	corpora/${FUZZER}-reduced corpora/${FUZZER} \
	-artifact_prefix="artifacts/${FUZZER}/" \
	-use_value_profile=1 \
	-print_final_stats=1 \
	-merge=1 \
	$EXTRA_ARGS

# Replace the original corpus directory with the reduced corpus directory.
rm -rf corpora/${FUZZER}
mv corpora/${FUZZER}-reduced corpora/${FUZZER}