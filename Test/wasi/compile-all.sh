#!/bin/bash

set -e

TEST_DIR=$(cd `dirname $0` && pwd)
WASI_SDK_DIR=$1

find $TEST_DIR -iname "*.cpp" | xargs -n1 $TEST_DIR/compile.sh $WASI_SDK_DIR
