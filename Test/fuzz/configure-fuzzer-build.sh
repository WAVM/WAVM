#!/bin/bash

set -e

BUILD_DIR=$(pwd)
WAVM_DIR=$(cd `dirname $0`/../.. && pwd)

clang -v
clang++ -v

rm -rf llvm6
mkdir llvm6
git clone -b release_60 http://github.com/llvm-mirror/llvm llvm6

mkdir $BUILD_DIR/llvm6/build
cd $BUILD_DIR/llvm6/build
CC=clang CXX=clang++ cmake \
	-G Ninja \
	-DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DLLVM_USE_SANITIZER="Address;Undefined" \
	-DLLVM_USE_SANITIZE_COVERAGE=1 \
	-DLLVM_TARGETS_TO_BUILD=X86 \
	-DLLVM_INCLUDE_TOOLS=0 \
	-DLLVM_INCLUDE_TESTS=0 \
	..
ninja

cd $BUILD_DIR
CC=clang CXX=clang++ cmake \
	-G Ninja \
	-DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DENABLE_ASAN=1 \
	-DENABLE_LIBFUZZER=1 \
	-DLLVM_DIR=$BUILD_DIR/llvm6/build/lib/cmake/llvm \
	$WAVM_DIR
ninja
