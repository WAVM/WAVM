#!/bin/sh

set -e

mkdir release
cd release
cmake .. -DCMAKE_BUILD_TYPE=RELEASE
make
ctest -V

cd ..
mkdir debug
cd debug
cmake .. -DCMAKE_BUILD_TYPE=DEBUG
make
