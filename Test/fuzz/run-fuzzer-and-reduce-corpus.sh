#!/bin/bash

set -e

rm artifacts/$1/* || true

SCRIPT_DIR=$(cd `dirname $0` && pwd)
$SCRIPT_DIR/run-fuzzer.sh ${@:1} || true
$SCRIPT_DIR/reduce-corpus.sh ${@:1} || true

cd corpora
git add *
git commit -m "Update $1" -q || true
git push

cd ../artifacts
git add *
git commit -m "Update $1" || true
git push

cd ..
