#!/bin/bash
# Build the plugin and copy artefacts into bin/<version>/.
# Version comes from CMakeLists.txt (project(... VERSION x.y.z ...)).
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

VERSION="$(grep -E '^project\(XSampler VERSION' CMakeLists.txt | sed -E 's/.*VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/')"
if [ -z "$VERSION" ]; then
  echo "Could not parse version from CMakeLists.txt" >&2
  exit 1
fi

echo "Building XSampler $VERSION…"
./build.sh

OUT="bin/$VERSION"
rm -rf "$OUT"
mkdir -p "$OUT"

cp -R build/XSampler_artefacts/Release/VST3/XSampler.vst3        "$OUT/"
cp -R build/XSampler_artefacts/Release/AU/XSampler.component     "$OUT/"
cp -R build/XSampler_artefacts/Release/Standalone/XSampler.app   "$OUT/"

echo
echo "✅ Released $VERSION → $OUT"
ls -1 "$OUT"
