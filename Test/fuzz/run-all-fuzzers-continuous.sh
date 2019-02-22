#!/bin/bash

set -e

SCRIPT_DIR=$(cd `dirname $0` && pwd)

while true; do $SCRIPT_DIR/run-all-fuzzers.sh; done
