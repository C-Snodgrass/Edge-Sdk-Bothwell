#!/bin/bash

# Create build directory and navigate into it
rm -rf build && mkdir -p build && cd build

# Run cmake and make
cmake .. && make -j4
