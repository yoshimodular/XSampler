#!/bin/bash
set -e
cmake -B build-ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
  -DXSAMPLER_BUILD_TESTS=ON \
  -G Ninja
cmake --build build-ninja --target XSamplerTests -j$(sysctl -n hw.logicalcpu)

# Locate and run the test binary (juce_add_console_app puts it under the target dir).
TEST_BIN="$(find build-ninja -type f -name XSamplerTests -perm +111 | head -n 1)"
if [ -z "$TEST_BIN" ]; then
  echo "Could not locate XSamplerTests binary" >&2
  exit 1
fi
echo "Running: $TEST_BIN"
"$TEST_BIN"
