#!/bin/sh

set -e -v

if [ "$CXX" = "g++" ]; then
  export CXXFLAGS="-fuse-ld=gold";
fi

echo $CXX
$CXX --version

cmake --version

# Download a binary build of LLVM (not available in Travis's whitelisted apt sources)
mkdir llvm
cd llvm
wget --no-check-certificate --quiet -O ./llvm.tar.xz ${LLVM_URL}
tar --strip-components=1 -xf ./llvm.tar.xz
export LLVM_DIR=`pwd`/lib/cmake/llvm
cd ..

echo $ENABLE_RELEASE
if [ "$ENABLE_RELEASE" = "YES" ]; then
# Build and test a release build of WAVM.
mkdir release
cd release
  cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo \
           -DLLVM_DIR=${LLVM_DIR} \
           -DWAVM_ENABLE_RUNTIME=${ENABLE_RUNTIME} \
           -DWAVM_ENABLE_STATIC_LINKING=${ENABLE_STATIC_LINKING} \
           -DWAVM_ENABLE_RELEASE_ASSERTS=${ENABLE_RELEASE_ASSERTS} \
           -DWAVM_ENABLE_ASAN=${ENABLE_ASAN} \
           -DWAVM_ENABLE_UBSAN=${ENABLE_UBSAN} \
           -DWAVM_ENABLE_LIBFUZZER=${ENABLE_LIBFUZZER} \
           -DWAVM_ENABLE_TSAN=${ENABLE_TSAN} \
           -DWAVM_ENABLE_UNWIND=${ENABLE_UNWIND}
  make -j2
  ctest -V -j2
  
  if [ "$ENABLE_RUNTIME" = "YES" ]; then
    bin/wavm test benchmark
  fi

  cd ..
fi

echo $ENABLE_DEBUG
if [ "$ENABLE_DEBUG" = "YES" ]; then
  # Build and test a debug build of WAVM.
  mkdir debug
  cd debug
  cmake .. -DCMAKE_BUILD_TYPE=Debug \
           -DLLVM_DIR=${LLVM_DIR} \
           -DWAVM_ENABLE_RUNTIME=${ENABLE_RUNTIME} \
           -DWAVM_ENABLE_STATIC_LINKING=${ENABLE_STATIC_LINKING} \
           -DWAVM_ENABLE_RELEASE_ASSERTS=${ENABLE_RELEASE_ASSERTS} \
           -DWAVM_ENABLE_ASAN=${ENABLE_ASAN} \
           -DWAVM_ENABLE_UBSAN=${ENABLE_UBSAN} \
           -DWAVM_ENABLE_LIBFUZZER=${ENABLE_LIBFUZZER} \
           -DWAVM_ENABLE_TSAN=${ENABLE_TSAN} \
           -DWAVM_ENABLE_UNWIND=${ENABLE_UNWIND}
  make -j2
  ctest -V -j2
  cd ..
fi