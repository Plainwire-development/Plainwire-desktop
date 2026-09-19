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

rc_add_path() {
  local rc="$1"
  local tmp w1 w2 newline
  w1="${BIN_DIR}:\$PATH"
  w2=""
  if [[ "$PREFIX" == "$HOME/"* ]]; then
    w2="\$HOME/${PREFIX#"$HOME/"}/bin:\$PATH"
  fi
  newline="export PATH=\"${BIN_DIR}:\$PATH\""

  tmp="$(mktemp "$(dirname "$rc")/$(basename "$rc").XXXXXX")" 2>/dev/null || return 1

  # Rewrite the profile through a temp file: drop every previous Plainwire PATH
  # entry, then append one fresh line. The temp file is renamed into place only
  # when complete, so the profile can never be left truncated on failure.
  awk -v W1="$w1" -v W2="$w2" '
    function is_plainwire(line,     val) {
      if (line !~ /^export PATH=/) return 0
      val = line
      sub(/^export PATH="/, "", val)
      sub(/"$/, "", val)
      return (val == W1 || (W2 != "" && val == W2))
    }
    /^# Add Plainwire bin directory to PATH[[:space:]]*$/ { skip = 1; next }
    skip == 1 { skip = 0; next }
    is_plainwire($0) { next }
    { print }
  ' "$rc" > "$tmp" || { rm -f "$tmp"; return 1; }

  printf '\n# Add Plainwire bin directory to PATH\n%s\n' "$newline" >> "$tmp"
  if ! mv -f "$tmp" "$rc"; then
    rm -f "$tmp"
    return 1
  fi
}

if [[ "$in_path" -eq 1 ]]; then
  echo "==> $BIN_DIR is already on your PATH, you can run 'plainwire-desktop'."
else
  echo
  echo "==> $BIN_DIR is not on your PATH, so 'plainwire-desktop' won't be"
  echo "    found from the terminal unless you use the full path."
  echo "    Adding it to your shell profile is automatic and easy to reverse."
  read -rp "==> Add $BIN_DIR to your PATH automatically? [Y/n] " answer
  case "${answer:-y}" in
    y|Y|yes|YES|Yes)
      shell_name="$(basename "${SHELL:-$0}")"
      case "$shell_name" in
        bash) rc="$HOME/.bashrc" ;;
        zsh)  rc="$HOME/.zshrc" ;;
        *)    rc="$HOME/.profile" ;;
      esac
      if [[ -f "$rc" ]] && grep -qF "export PATH=\"$BIN_DIR:\$PATH\"" "$rc"; then
        echo "==> $BIN_DIR is already set up in $rc, nothing to do."
      else
        rc_has_entry=0
        if [[ -f "$rc" ]] &&
           { grep -q '^# Add Plainwire bin directory to PATH' "$rc" ||
             grep -qF "export PATH=\"$BIN_DIR:\$PATH\"" "$rc"; }; then
          rc_has_entry=1
        fi
        if rc_add_path "$rc"; then
          if [[ "$rc_has_entry" -eq 1 ]]; then
            echo "==> Replaced the existing $BIN_DIR entry in $rc with the current one,"
            echo "    written atomically so $rc can never be left half-edited."
          else
            echo "==> Added the PATH entry for $BIN_DIR to $rc."
          fi
          echo "    New terminals can run 'plainwire-desktop' right away."
          echo "    To use it in this terminal now: source $rc"
        else
          echo "==> Could not update $rc (left untouched). You can still run: $BIN_DIR/plainwire-desktop"
        fi
      fi
      ;;
    *)
      echo "==> Skipped. Launch Plainwire with: $BIN_DIR/plainwire-desktop"
      ;;
  esac
fi
