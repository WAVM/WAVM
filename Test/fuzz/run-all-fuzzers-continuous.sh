#!/bin/bash

set -e

BUILD_DIR=$(pwd)
WAVM_DIR=$(cd `dirname $0`/../.. && pwd)
SCRIPT_DIR=$WAVM_DIR/Test/fuzz

while true; do

	# Pull the latest WAVM code
	cd $WAVM_DIR
	git pull
	cd $BUILD_DIR

	$SCRIPT_DIR/run-all-fuzzers.sh
done
