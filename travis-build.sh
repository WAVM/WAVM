#!/bin/sh

mkdir release
cd release
cmake .. -DCMAKE_BUILD_TYPE=RELEASE
make

cd ..
mkdir debug
cd debug
cmake .. -DCMAKE_BUILD_TYPE=DEBUG
make