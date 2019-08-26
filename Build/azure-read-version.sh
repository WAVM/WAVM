#!/bin/bash

VERSION=$(cat VERSION)
echo "##vso[task.setvariable variable=VERSION]$VERSION"