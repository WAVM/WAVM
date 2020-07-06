#!/bin/bash

set -e

rm artifacts/$1/* || true

SCRIPT_DIR=$(cd `dirname $0` && pwd)
$SCRIPT_DIR/run-fuzzer.sh ${@:1} || true
$SCRIPT_DIR/reduce-corpus.sh ${@:1} || true

cd corpora
git add *
git commit --amend -m "Reinitialize" -q || true
git prune
git push -f

cd ../corpora-archive
git add *
git commit --amend -m "Reinitialize" -q || true
git prune
#git push -f

cd ../artifacts
git add *
git commit -m "Update $1" || true
git push

cd ..
