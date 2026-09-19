#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PLAINWIRE_BUILD_DIR:-$ROOT/build}"
PREFIX="${PLAINWIRE_PREFIX:-$HOME/.local}"
BIN_DIR="$PREFIX/bin"

echo "==> Installing Plainwire to $BIN_DIR/plainwire-desktop"

if [[ ! -x "$BUILD_DIR/plainwire-desktop" ]]; then
  echo "==> Binary not found in $BUILD_DIR, building it first..."
  "$ROOT/build.sh"
fi

echo "==> Copying files into $PREFIX ..."
cmake --install "$BUILD_DIR" --prefix "$PREFIX"

if command -v update-desktop-database >/dev/null 2>&1; then
  echo "==> Updating desktop database..."
  update-desktop-database "$PREFIX/share/applications" >/dev/null 2>&1 || true
fi
if command -v xdg-mime >/dev/null 2>&1; then
  echo "==> Registering plainwire:// as the URL handler..."
  xdg-mime default me.kokonico.plainwire.desktop x-scheme-handler/plainwire >/dev/null 2>&1 || true
fi

printf '==> Installed Plainwire to %s/bin/plainwire-desktop\n' "$PREFIX"

case ":$PATH:" in
  *":$BIN_DIR:"*) in_path=1 ;;
  *) in_path=0 ;;
esac

if [[ "$in_path" -eq 1 ]]; then
  echo "==> $BIN_DIR is already on your PATH, you can run 'plainwire-desktop'."
else
  echo
  echo "==> $BIN_DIR is not on your PATH, so 'plainwire-desktop' won't be"
  echo "    found from the terminal unless you use the full path."
  read -rp "==> Add $BIN_DIR to your PATH? [y/N] " answer
  case "${answer:-n}" in
    y|Y|yes|YES|Yes)
      shell_name="$(basename "${SHELL:-$0}")"
      case "$shell_name" in
        bash) rc="$HOME/.bashrc" ;;
        zsh)  rc="$HOME/.zshrc" ;;
        *)    rc="$HOME/.profile" ;;
      esac
      line="export PATH=\"$BIN_DIR:\$PATH\""
      if [[ -f "$rc" ]] && grep -qF "$BIN_DIR" "$rc"; then
        echo "==> PATH entry already present in $rc, nothing to do."
      else
        echo "==> Appending '$line' to $rc ..."
        printf '\n# Add Plainwire bin directory to PATH\n%s\n' "$line" >> "$rc"
        echo "==> Done. Run 'source $rc' (or restart your terminal) to pick it up."
      fi
      ;;
    *)
      echo "==> Skipped. Launch Plainwire with: $BIN_DIR/plainwire-desktop"
      ;;
  esac
fi
