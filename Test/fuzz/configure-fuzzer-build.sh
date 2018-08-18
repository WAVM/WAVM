#!/bin/bash

set -e

BUILD_DIR=$(pwd)
WAVM_DIR=$(cd `dirname $0`/../.. && pwd)

clang -v
clang++ -v

#rm -rf llvm7
#mkdir llvm7
#git clone -b release_70 http://github.com/llvm-mirror/llvm llvm7

#mkdir $BUILD_DIR/llvm7/build
#cd $BUILD_DIR/llvm7/build
#CC=clang CXX=clang++ cmake \
#	-G Ninja \
#	-DCMAKE_BUILD_TYPE=RelWithDebInfo \
#	-DLLVM_USE_SANITIZER=Address \
#	-DLLVM_TARGETS_TO_BUILD=X86 \
#	..
#ninja

cd $BUILD_DIR
CC=clang CXX=clang++ cmake \
	-G Ninja \
	-DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DENABLE_ASAN=1 \
	-DENABLE_LIBFUZZER=1 \
	-DLLVM_DIR=./llvm7/build/lib/cmake/llvm \
	$WAVM_DIR
ninja
