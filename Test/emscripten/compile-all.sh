#!/bin/bash

set -e

TEST_DIR=$(cd `dirname $0` && pwd)

$TEST_DIR/compile.sh args.cpp
$TEST_DIR/compile.sh clock.cpp
$TEST_DIR/compile.sh exit.cpp
$TEST_DIR/compile.sh stdout.cpp
