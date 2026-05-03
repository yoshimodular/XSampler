#!/bin/bash
set -e
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
  -DXSAMPLER_BUILD_TESTS=OFF \
  -G Ninja
cmake --build build --config Release -j$(sysctl -n hw.logicalcpu)
