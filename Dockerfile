FROM ubuntu:18.04

RUN apt-get update
RUN apt-get install -y software-properties-common

# System deps
RUN apt-get update
RUN apt-get install -y \
    autoconf \
    automake \
    build-essential \
    cmake \
    libtool \
    llvm-6.0 \
    make \
    ninja-build \
    sudo \
    unzip \
    zlib1g-dev

RUN apt-get clean autoclean
RUN apt-get autoremove -y

# Copy this code into place
COPY . /code

# Create a build directory
WORKDIR /build

RUN cmake -G Ninja /code -DCMAKE_BUILD_TYPE=RelWithDebInfo
RUN ninja

CMD /bin/bash
