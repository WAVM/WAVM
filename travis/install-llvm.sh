#!/bin/sh

echo "yes" | sudo add-apt-repository 'deb http://ppa.launchpad.net/ubuntu-toolchain-r/test/ubuntu precise main'
echo "yes" | sudo add-apt-repository 'deb http://llvm.org/apt/precise/ llvm-toolchain-precise-3.7 main'

sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 1E9377A2BA9EF27F
wget -O - http://llvm.org/apt/llvm-snapshot.gpg.key | sudo apt-key add -

sudo apt-get update
sudo apt-get install -y libllvm3.7 libllvm3.7-dbg llvm-3.7 llvm-3.7-dev llvm-3.7-runtime
