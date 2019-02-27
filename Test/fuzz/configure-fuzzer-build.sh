#!/bin/bash

set -e

BUILD_DIR=$(pwd)
WAVM_DIR=$(cd `dirname $0`/../.. && pwd)

clang-7 -v
clang++-7 -v

rm -rf llvm
mkdir llvm
git clone http://github.com/llvm/llvm-project llvm

rm -rf $BUILD_DIR/llvm/build 
mkdir $BUILD_DIR/llvm/build
cd $BUILD_DIR/llvm/build
CC=clang-7 CXX=clang++-7 cmake \
	-G Ninja \
	-DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DLLVM_USE_SANITIZER="Address;Undefined" \
	-DLLVM_USE_SANITIZE_COVERAGE=1 \
	-DLLVM_TARGETS_TO_BUILD=X86 \
	-DLLVM_INCLUDE_TOOLS=0 \
	-DLLVM_INCLUDE_TESTS=0 \
	../llvm
ninja

cd $BUILD_DIR
CC=clang-7 CXX=clang++-7 cmake \
	-G Ninja \
	-DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DWAVM_ENABLE_ASAN=1 \
	-DWAVM_ENABLE_UBSAN=1 \
	-DWAVM_ENABLE_LIBFUZZER=1 \
	-DWAVM_ENABLE_RELEASE_ASSERTS=1 \
	-DLLVM_DIR=$BUILD_DIR/llvm/build/lib/cmake/llvm \
	$WAVM_DIR
ninja
