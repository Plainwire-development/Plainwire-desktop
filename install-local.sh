#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PLAINWIRE_BUILD_DIR:-$ROOT/build}"
PREFIX="${PLAINWIRE_PREFIX:-$HOME/.local}"

if [[ ! -x "$BUILD_DIR/plainwire-desktop" ]]; then
  "$ROOT/build.sh"
fi

cmake --install "$BUILD_DIR" --prefix "$PREFIX"

if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database "$PREFIX/share/applications" >/dev/null 2>&1 || true
fi
if command -v xdg-mime >/dev/null 2>&1; then
  xdg-mime default me.kokonico.plainwire.desktop x-scheme-handler/plainwire >/dev/null 2>&1 || true
fi

printf 'Installed Plainwire to %s/bin/plainwire-desktop\n' "$PREFIX"
