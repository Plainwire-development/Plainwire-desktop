#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PLAINWIRE_BUILD_DIR:-$ROOT/build}"
BUILD_TYPE="${PLAINWIRE_BUILD_TYPE:-Release}"

cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_SUPPRESS_REGENERATION=ON
cmake --build "$BUILD_DIR" --parallel

printf '\nBuilt: %s\n' "$BUILD_DIR/plainwire-desktop"
