FROM ubuntu:22.04

# Configure APT repositories
ARG LLVM_VERSION_MAJOR=17
RUN apt update \
    && apt upgrade -y \
    && apt install -y \
        curl \
        gpg \
        software-properties-common \
        wget \
    # LLVM APT Repo config
    && wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc \
    && add-apt-repository -y "deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-${LLVM_VERSION_MAJOR} main"


# System deps
RUN apt update \
    && apt install -y \
        autoconf \
        automake \
        build-essential \
        cmake \
        libtool \
        llvm-17 \
        make \
        ninja-build \
        sudo \
        unzip \
        vim \
        zlib1g-dev \
    && apt clean autoclean \
    && apt autoremove -y

# Copy this code into place
COPY . /code

# Create a build directory
WORKDIR /build

RUN cmake -G Ninja /code -DCMAKE_BUILD_TYPE=RelWithDebInfo
RUN ninja

CMD /bin/bash
