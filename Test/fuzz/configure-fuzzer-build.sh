#!/bin/bash

set -e

BUILD_DIR=$(pwd)
WAVM_DIR=$(cd `dirname $0`/../.. && pwd)

clang -v
clang++ -v

rm -rf llvm7
mkdir llvm7
git clone -b release_70 http://github.com/llvm-mirror/llvm llvm7

mkdir $BUILD_DIR/llvm7/build
cd $BUILD_DIR/llvm7/build
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
	-DWAVM_ENABLE_ASAN=1 \
	-DWAVM_ENABLE_UBSAN=1 \
	-DWAVM_ENABLE_LIBFUZZER=1 \
	-DLLVM_DIR=$BUILD_DIR/llvm6/build/lib/cmake/llvm \
	$WAVM_DIR
ninja
