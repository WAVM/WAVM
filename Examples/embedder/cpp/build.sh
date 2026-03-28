#!/bin/sh

WAVM_INSTALL_DIR=/usr

# Compile embedder-example.cpp to embedder-example.o
c++ \
	-DWASM_C_API="" \
	-DWAVM_API="" \
	-g \
	-o embedder-example.cpp.o \
	-lWAVM \
	embedder-example.cpp

# Link embedder-example.o with libWAVM.so
c++ \
	-g \
	-o embedder-example \
	-Wl,-rpath,$WAVM_BUILD_DIR \
	embedder-example.cpp.o \
	$WAVM_INSTALL_DIR/lib/libWAVM.so.0