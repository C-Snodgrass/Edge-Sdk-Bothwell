#!/bin/bash

# Create build directory and navigate into it
mkdir -p build && cd build

# Run cmake and make
cmake .. && make -j4

