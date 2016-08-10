#!/bin/sh

set -e -v

if [[ $TRAVIS_OS_NAME == "osx" ]]; then
  export CMAKE_LLVM_DIR_ARGUMENT="-DLLVM_DIR=$(brew --prefix llvm38)/lib/llvm-3.8/share/llvm/cmake";
else
  export CMAKE_LLVM_DIR_ARGUMENT="-DLLVM_DIR=/usr/lib/llvm-3.8/share/llvm/cmake";
fi

mkdir release
cd release
cmake .. -DCMAKE_BUILD_TYPE=RELEASE $CMAKE_LLVM_DIR_ARGUMENT
make
ctest -V

cd ..
mkdir debug
cd debug
cmake .. -DCMAKE_BUILD_TYPE=DEBUG $CMAKE_LLVM_DIR_ARGUMENT
make
ASAN_OPTIONS=detect_leaks=0 ctest -V