#!/bin/sh

set -e -v

if [ "$CXX" = "g++" ]; then
  export CXX="g++-7" CC="gcc-7";
fi

if [ "$TRAVIS_OS_NAME" != "osx" ]; then
  export CXXFLAGS="-fuse-ld=gold";
  if [ "$CXX" = "clang++" ]; then
    export CXX="clang++-5.0" CC="clang-5.0";
  fi
fi

$CXX --version

if [ $TRAVIS_OS_NAME == "osx" ]; then
  export CMAKE_URL="https://cmake.org/files/v3.7/cmake-3.7.2-Darwin-x86_64.tar.gz";
  export LLVM_URL="http://releases.llvm.org/6.0.0/clang+llvm-6.0.0-x86_64-apple-darwin.tar.xz";
else
  export CMAKE_URL="https://cmake.org/files/v3.7/cmake-3.7.2-Linux-x86_64.tar.gz";
  export LLVM_URL="http://releases.llvm.org/6.0.0/clang+llvm-6.0.0-x86_64-linux-gnu-ubuntu-14.04.tar.xz";
fi


# Download a newer version of cmake than is available in Travis's whitelisted apt sources.
mkdir cmake
cd cmake
wget --no-check-certificate --quiet -O ./cmake.tar.gz ${CMAKE_URL}
tar --strip-components=1 -xzf ./cmake.tar.gz
export PATH=`pwd`/bin:${PATH}
cd ..
cmake --version

# Download a binary build of LLVM6 (also not available in Travis's whitelisted apt sources)
mkdir llvm6
cd llvm6
wget --no-check-certificate --quiet -O ./llvm.tar.xz ${LLVM_URL}
tar --strip-components=1 -xf ./llvm.tar.xz
export LLVM_DIR=`pwd`/lib/cmake/llvm
cd ..

# Build and test a release build of WAVM.
mkdir release
cd release
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo \
         -DLLVM_DIR=${LLVM_DIR} \
         -DENABLE_RUNTIME=${ENABLE_RUNTIME} \
         -DENABLE_STATIC_LINKING=${ENABLE_STATIC_LINKING} \
         -DENABLE_RELEASE_ASSERTS=${ENABLE_RELEASE_ASSERTS} \
         -DENABLE_ASAN=${ENABLE_ASAN} \
         -DENABLE_UBSAN=${ENABLE_UBSAN}
make -j2
ASAN_OPTIONS=detect_leaks=0 ctest -V -j2
cd ..

# Build and test a debug build of WAVM.
mkdir debug
cd debug
cmake .. -DCMAKE_BUILD_TYPE=Debug \
         -DLLVM_DIR=${LLVM_DIR} \
         -DENABLE_RUNTIME=${ENABLE_RUNTIME} \
         -DENABLE_STATIC_LINKING=${ENABLE_STATIC_LINKING} \
         -DENABLE_RELEASE_ASSERTS=${ENABLE_RELEASE_ASSERTS} \
         -DENABLE_ASAN=${ENABLE_ASAN} \
         -DENABLE_UBSAN=${ENABLE_UBSAN}
make -j2
ASAN_OPTIONS=detect_leaks=0 ctest -V -j2
cd ..