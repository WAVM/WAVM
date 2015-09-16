#!/bin/sh

echo "yes" | add-apt-repository 'deb http://llvm.org/apt/trusty/ llvm-toolchain-trusty-3.7 main'

wget -O - http://llvm.org/apt/llvm-snapshot.gpg.key | apt-key add -
apt-get update
apt-get install libllvm3.7 libllvm3.7-dbg llvm-3.7 llvm-3.7-dev llvm-3.7-runtime
