#!/bin/bash

set -e

BUILD_DIR=$(pwd)
WAVM_DIR=$(cd `dirname $0`/../.. && pwd)

clang-9 -v
clang++-9 -v
ld.lld-9 -v
llvm-ar-9 --version
llvm-ranlib-9 --version

rm -rf llvm
mkdir llvm
git clone http://github.com/llvm/llvm-project -b release/9.x llvm

rm -rf $BUILD_DIR/llvm/build 
mkdir $BUILD_DIR/llvm/build
cd $BUILD_DIR/llvm/build
cmake \
	-G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DLLVM_USE_SANITIZER="Address;Undefined" \
	-DLLVM_USE_SANITIZE_COVERAGE=ON \
	-DLLVM_OPTIMIZED_TABLEGEN=ON \
	-DLLVM_ENABLE_ASSERTIONS=ON \
	-DLLVM_TARGETS_TO_BUILD="X86;AArch64" \
    -DLLVM_INCLUDE_DOCS=OFF \
    -DLLVM_INCLUDE_EXAMPLES=OFF \
    -DLLVM_INCLUDE_GO_TESTS=OFF \
    -DLLVM_INCLUDE_TOOLS=OFF \
    -DLLVM_INCLUDE_UTILS=OFF \
    -DLLVM_INCLUDE_TESTS=OFF \
    -DLLVM_ENABLE_ZLIB=OFF \
    -DLLVM_ENABLE_TERMINFO=OFF \
	-DLLVM_USE_LINKER=lld-9 \
	-DCMAKE_C_COMPILER=clang-9 \
	-DCMAKE_CXX_COMPILER=clang++-9 \
	-DCMAKE_AR=`which llvm-ar-9` \
	-DCMAKE_RANLIB=`which llvm-ranlib-9` \
	-DCMAKE_C_FLAGS="-march=native" \
	-DCMAKE_CXX_FLAGS="-march=native" \
	../llvm
ninja

cd $BUILD_DIR
cmake \
	-G Ninja \
	-DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DWAVM_ENABLE_ASAN=ON \
	-DWAVM_ENABLE_UBSAN=ON \
	-DWAVM_ENABLE_LIBFUZZER=ON \
	-DWAVM_ENABLE_RELEASE_ASSERTS=ON \
	-DWAVM_ENABLE_STATIC_LINKING=ON \
	-DWAVM_USE_LINKER=lld-9 \
	-DCMAKE_C_COMPILER=clang-9 \
	-DCMAKE_CXX_COMPILER=clang++-9 \
	-DCMAKE_AR=`which llvm-ar-9` \
	-DCMAKE_RANLIB=`which llvm-ranlib-9` \
	-DCMAKE_C_FLAGS="-march=native" \
	-DCMAKE_CXX_FLAGS="-march=native" \
	-DLLVM_DIR=$BUILD_DIR/llvm/build/lib/cmake/llvm \
	$WAVM_DIR
ninja
