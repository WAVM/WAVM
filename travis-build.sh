#!/bin/sh

set -e -v

if [[ $TRAVIS_OS_NAME == "osx" ]]; then
  export CMAKE_LLVM_DIR_ARGUMENT="-DLLVM_DIR=$(brew --prefix llvm37)/lib/llvm-3.7/share/llvm/cmake";
else
  export CMAKE_LLVM_DIR_ARGUMENT="";
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
