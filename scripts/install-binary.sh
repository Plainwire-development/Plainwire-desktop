#!/usr/bin/env bash
set -euo pipefail

# Plainwire Desktop - prebuilt binary installer / updater
#
# Downloads the matching binary for your platform from the latest GitHub
# release, verifies its SHA-256 checksum, sanity-runs it, and then installs it
# atomically (backup + rollback on any failure) so the app is never left in a
# broken state, even if the download or replacement is interrupted.
#
# Usage:
#   install-binary.sh                 # install (or update, if already installed)
#   install-binary.sh update          # update an existing install
#   install-binary.sh check           # compare installed vs latest (no changes)
#   install-binary.sh -y              # assume yes for every prompt (CI-friendly)
#
# Environment:
#   PLAINWIRE_REPO     GitHub repo to fetch from (default Plainwire-development/Plainwire-desktop)
#   PLAINWIRE_PREFIX   install prefix (default $HOME/.local)
#   PLAINWIRE_ARCH     override detected architecture (amd64 | arm64)
#   PLAINWIRE_API      override the "latest release" JSON URL (advanced/testing)
#   PLAINWIRE_ASSETS   override the binary download base URL (advanced/testing)

REPO="${PLAINWIRE_REPO:-Plainwire-development/Plainwire-desktop}"
RELEASES_URL="${PLAINWIRE_API:-https://api.github.com/repos/$REPO/releases/latest}"
DOWNLOADS_URL="${PLAINWIRE_ASSETS:-https://github.com/$REPO/releases/latest/download}"
PREFIX="${PLAINWIRE_PREFIX:-$HOME/.local}"
INSTALL_BIN="$PREFIX/bin/plainwire-desktop"
BACKUP_BIN="$PREFIX/lib/plainwire/plainwire-desktop.old"
STATE_FILE="$PREFIX/lib/plainwire/installed"
DESKTOP_FILE="me.kokonico.plainwire.desktop"
ICON_FILE="plainwire.png"
MODE="install"
ASSUME_YES=0
INTERACTIVE=0
VERSION=""

if [[ -t 0 ]] && [[ -t 1 ]]; then
  INTERACTIVE=1
fi

usage() {
  sed -nE 's/^# ?//p' "${BASH_SOURCE[0]}" | sed -nE '/^Plainwire Desktop/,/^Environment:/p' | head -n 40
}

die() {
  printf 'ERROR: %s\n' "$*" >&2
  exit 1
}

warn() {
  printf 'NOTE: %s\n' "$*" >&2
}

have() {
  command -v "$1" >/dev/null 2>&1
}

prompt_yn() {
  local msg="$1" default="${2:-n}" ans
  if [[ "$ASSUME_YES" -eq 1 ]]; then
    return 0
  fi
  if [[ "$INTERACTIVE" -eq 0 ]]; then
    return 1
  fi
  read -rp "$msg " ans
  case "${ans:-$default}" in
    y|Y|yes|YES|Yes) return 0 ;;
    *) return 1 ;;
  esac
}

detect_platform() {
  case "$(uname -s)" in
    Linux) ;;
    *)
      die "Prebuilt binaries are currently only built for Linux (you are on $(uname -s))."
        "Build from source instead: https://github.com/$REPO -> ./install-local.sh"
      ;;
  esac

  local machine
  machine="$(uname -m)"
  case "$machine" in
    x86_64|amd64) ARCH="amd64" ;;
    aarch64|arm64) ARCH="arm64" ;;
    *)
      die "No prebuilt binary for architecture '$machine'. Build from source instead: ./install-local.sh"
      ;;
  esac

  ARCH="${PLAINWIRE_ARCH:-$ARCH}"
  echo "==> Platform: Linux $ARCH"
}

fetch_latest_tag() {
  local tag
  tag="$(curl -fsSL "$RELEASES_URL" 2>/dev/null |
    sed -nE 's/.*"tag_name"[[:space:]]*:[[:space:]]*"([^"]+)".*/\1/p' | head -n1 || true)"
  [[ -n "$tag" ]] || die "Could not reach GitHub to find the latest release. Check your network, or override the repo with PLAINWIRE_REPO."
  printf '%s' "$tag"
}

# Report the version of an existing *Plainwire* binary at the install path.
# Returns nothing when the file there is missing, not executable, or is some
# other program that happens to share the name - so unrelated apps in
# local/bin are never treated as a Plainwire install.
installed_version() {
  local out
  [[ -x "$INSTALL_BIN" ]] || return 0
  out="$("$INSTALL_BIN" --version 2>/dev/null | tail -n1 || true)"
  case "$out" in
    *Plainwire*)
      printf '%s' "$out" | grep -oE '[0-9]+(\.[0-9]+)+' | head -n1 || true
      ;;
  esac
}

# Authoritative record that the prebuilt installer put a binary here, written
# only after a fully verified swap so it can never mark a broken install.
write_state() {
  local ver="$1" tmp
  install -d "$(dirname "$STATE_FILE")"
  tmp="$(mktemp "$STATE_FILE.XXXXXX")" 2>/dev/null || return 1
  printf 'version=%s\ninstalled_by=prebuilt\n' "$ver" > "$tmp"
  if ! mv -f "$tmp" "$STATE_FILE"; then
    rm -f "$tmp"
    return 1
  fi
}

check_qt_runtime() {
  if ldconfig -p 2>/dev/null | grep -q 'libQt6WebEngineCore\.so\.6'; then
    return 0
  fi
  warn "This prebuilt binary needs Qt 6 WebEngine runtime libraries, which we could not find"
  warn "on your system (libQt6WebEngineCore.so.6). You can install them with something like:"
  warn "  sudo apt install qt6-base-dev qt6-webengine-dev"
  if ! prompt_yn "Install anyway? [y/N]"; then
    die "Aborted. Install the Qt 6 libraries, or build from source with ./install-local.sh"
  fi
}

download_and_verify() {
  local tarball="plainwire-desktop-linux-$ARCH.tar.gz"

  echo "==> Downloading $tarball ..."
  curl -fsSL "$DOWNLOADS_URL/$tarball" -o "$TMP/$tarball" ||
    die "Download failed. Check your network, or verify this release really contains '$tarball'."
  curl -fsSL "$DOWNLOADS_URL/SHA256SUMS.txt" -o "$TMP/SHA256SUMS.txt" 2>/dev/null || true

  if [[ -f "$TMP/SHA256SUMS.txt" ]] && grep -q "$tarball" "$TMP/SHA256SUMS.txt"; then
    if ( cd "$TMP" && sha256sum -c --ignore-missing --status SHA256SUMS.txt ); then
      echo "==> Checksum OK."
    else
      die "Checksum verification failed. Refusing a corrupt download - please try again."
    fi
  else
    die "Could not verify the checksum. Refusing to continue for safety."
  fi

  if tar -tzf "$TMP/$tarball" | grep -qE '^/'; then
    die "Refusing to extract a tarball that contains absolute paths."
  fi

  mkdir -p "$TMP/extract"
  tar -xzf "$TMP/$tarball" -C "$TMP/extract"
  [[ -x "$TMP/extract/plainwire-desktop" ]] ||
    die "The archive does not contain a 'plainwire-desktop' binary."

  echo "==> Sanity-running the downloaded binary ..."
  VERSION="$("$TMP/extract/plainwire-desktop" --version 2>&1 | tail -n1 |
    grep -oE '[0-9]+(\.[0-9]+)+' | head -n1 || true)" &&
    [[ -n "$VERSION" ]] ||
    die "The downloaded binary could not run on your system. It may be incompatible - you can build from source instead."
  echo "==> Downloaded binary reports version $VERSION."
}

BACKUP_RESTORED=0
restore_backup() {
  [[ "$BACKUP_RESTORED" -eq 0 ]] || return 0
  if [[ -f "$BACKUP_BIN" ]]; then
    warn "Rolling back to the previous binary ..."
    mv -f "$BACKUP_BIN" "$INSTALL_BIN" 2>/dev/null || true
  else
    rm -f "$INSTALL_BIN" 2>/dev/null || true
  fi
  BACKUP_RESTORED=1
}

install_desktop_files() {
  install -Dm 644 "$TMP/extract/$DESKTOP_FILE" "$PREFIX/share/applications/$DESKTOP_FILE" 2>/dev/null || {
    warn "Could not install the .desktop file (optional)."
  }
  install -Dm 644 "$TMP/extract/$ICON_FILE" "$PREFIX/share/icons/hicolor/256x256/apps/$ICON_FILE" 2>/dev/null || {
    warn "Could not install the icon (optional)."
  }
}

refresh_desktop_db() {
  if have update-desktop-database; then
    update-desktop-database "$PREFIX/share/applications" >/dev/null 2>&1 || true
  fi
  if have xdg-mime; then
    xdg-mime default "$DESKTOP_FILE" x-scheme-handler/plainwire >/dev/null 2>&1 || true
  fi
}

swap_binary() {
  local staging="$1" had_old=0
  install -d "$PREFIX/bin" "$PREFIX/lib/plainwire"

  if [[ -e "$INSTALL_BIN" ]]; then
    had_old=1
    mv -f "$INSTALL_BIN" "$BACKUP_BIN" ||
      die "Could not set the current binary aside."
  fi

  if ! install -m 755 "$staging" "$INSTALL_BIN.new.$$"; then
    if [[ "$had_old" -eq 1 ]]; then
      mv -f "$BACKUP_BIN" "$INSTALL_BIN" 2>/dev/null || true
    fi
    die "Could not stage the new binary."
  fi

  if ! mv -f "$INSTALL_BIN.new.$$" "$INSTALL_BIN"; then
    if [[ "$had_old" -eq 1 ]]; then
      mv -f "$BACKUP_BIN" "$INSTALL_BIN" 2>/dev/null || true
    fi
    rm -f "$INSTALL_BIN.new.$$" 2>/dev/null || true
    die "Could not move the new binary into place."
  fi
}

rc_add_path() {
  local rc="$1"
  local tmp w1 w2 newline
  w1="${PREFIX}/bin:\$PATH"
  w2=""
  if [[ "$PREFIX" == "$HOME/"* ]]; then
    w2="\$HOME/${PREFIX#"$HOME/"}/bin:\$PATH"
  fi
  newline="export PATH=\"${PREFIX}/bin:\$PATH\""

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

maybe_add_to_path() {
  if [[ ":$PATH:" == *":$PREFIX/bin:"* ]]; then
    echo "==> $PREFIX/bin is already on your PATH; you can run 'plainwire-desktop'."
    return 0
  fi

  echo
  echo "==> The install directory $PREFIX/bin is not on your PATH yet, so"
  echo "    'plainwire-desktop' would only work if you use the full path every time."
  echo "    Adding it to your shell profile is automatic and easy to reverse."

  if prompt_yn "Add $PREFIX/bin to your PATH automatically? [Y/n]" y; then
    local shell_name rc rc_has_entry
    shell_name="$(basename "${SHELL:-/bin/sh}")"
    case "$shell_name" in
      bash) rc="$HOME/.bashrc" ;;
      zsh) rc="$HOME/.zshrc" ;;
      *) rc="$HOME/.profile" ;;
    esac

    if [[ -f "$rc" ]] && grep -qF "export PATH=\"$PREFIX/bin:\$PATH\"" "$rc"; then
      echo "==> $PREFIX/bin is already set up in $rc, nothing to do."
      return 0
    fi

    rc_has_entry=0
    if [[ -f "$rc" ]] &&
       { grep -q '^# Add Plainwire bin directory to PATH' "$rc" ||
         grep -qF "export PATH=\"$PREFIX/bin:\$PATH\"" "$rc"; }; then
      rc_has_entry=1
    fi

    if rc_add_path "$rc"; then
      if [[ "$rc_has_entry" -eq 1 ]]; then
        echo "==> Replaced the existing $PREFIX/bin entry in $rc with the current one,"
        echo "    written atomically so $rc can never be left half-edited."
      else
        echo "==> Added the PATH entry for $PREFIX/bin to $rc."
      fi
      echo "    New terminals can run 'plainwire-desktop' right away."
      echo "    To use it in this terminal now: source $rc"
    else
      warn "Could not update $rc (left untouched). You can still run: $INSTALL_BIN"
    fi
  else
    echo "==> Skipped. Launch Plainwire with: $INSTALL_BIN"
    echo "    (or add $PREFIX/bin to your PATH to run 'plainwire-desktop')."
  fi
}

install_new_version() {
  local new_ver="$1" mode="$2"

  if [[ "$mode" == "update" ]]; then
    echo "==> Found current version: $(installed_version)"
    if ! prompt_yn "Update to Plainwire $new_ver? [y/N]"; then
      echo "Cancelled. Nothing was changed."
      exit 1
    fi
  elif [[ "$mode" == "reinstall" ]]; then
    echo "==> Found Plainwire $(installed_version) already installed at $INSTALL_BIN."
    if ! prompt_yn "Replace it with a fresh copy of $new_ver (settings are kept)? [Y/n]" y; then
      echo "==> Keeping the current install."
      maybe_add_to_path
      exit 0
    fi
  elif [[ "$mode" == "replace" ]]; then
    echo
    echo "==> There is already a program at $INSTALL_BIN that is not a"
    echo "    Plainwire binary. It will be moved aside first and restored"
    echo "    automatically if anything goes wrong, so it cannot be lost."
    if ! prompt_yn "Replace it with Plainwire $new_ver? [y/N]"; then
      echo "Cancelled. Nothing was changed."
      exit 1
    fi
  else
    if ! prompt_yn "Install Plainwire $new_ver into $PREFIX? [y/N]"; then
      echo "Cancelled. You can still build from source with ./install-local.sh"
      exit 1
    fi
  fi

  check_qt_runtime

  TMP="$(mktemp -d)"
  trap 'restore_backup; rm -rf "$TMP"' EXIT
  BACKUP_RESTORED=0

  download_and_verify

  echo "==> Installing to $PREFIX ..."
  install_desktop_files
  swap_binary "$TMP/extract/plainwire-desktop"
  # After a successful swap: for a replace, keep the previous (non-Plainwire)
  # program preserved; otherwise drop the rollback copy of the old Plainwire.
  if [[ "$mode" == "replace" ]]; then
    mv -f "$BACKUP_BIN" "$PREFIX/lib/plainwire/previous-non-plainwire.bak" 2>/dev/null || true
  else
    rm -f "$BACKUP_BIN"
  fi
  BACKUP_RESTORED=1
  refresh_desktop_db
  write_state "$new_ver" || warn "Could not write the install state marker (the binary itself is installed and working)."

  if [[ "$mode" == "update" ]]; then
    echo "==> Done: Plainwire updated to $new_ver."
  elif [[ "$mode" == "reinstall" ]]; then
    echo "==> Done: refreshed Plainwire $new_ver at $INSTALL_BIN."
  elif [[ "$mode" == "replace" ]]; then
    echo "==> Done: replaced the other program at $INSTALL_BIN with Plainwire $new_ver."
  else
    echo "==> Done: installed Plainwire $new_ver to $PREFIX."
  fi
  echo "==> Launch it with: plainwire-desktop"
  maybe_add_to_path
}

main() {
  for arg in "$@"; do
    case "$arg" in
      install|update|check) MODE="$arg" ;;
      -y|--yes) ASSUME_YES=1 ;;
      -h|--help) usage; exit 0 ;;
      *) die "Unknown argument: $arg (install | update | check | -y | --help)" ;;
    esac
  done

  have curl || die "curl is required by this installer. Install it first (e.g. 'sudo apt install curl')."

  detect_platform

  echo "==> Plainwire Desktop installer for $REPO"
  echo "==> Checking for the latest release ..."
  local latest
  latest="$(fetch_latest_tag)"
  local latest_no_v="${latest#v}"

  local cur=""
  if [[ -x "$INSTALL_BIN" ]] || [[ -f "$STATE_FILE" ]]; then
    cur="$(installed_version)"
  fi

  case "$MODE" in
    check)
      if [[ -n "$cur" ]]; then
        echo "Installed: $cur"
        echo "Latest:    $latest_no_v"
        if [[ "$cur" == "$latest_no_v" ]]; then
          echo "You are up to date."
          exit 0
        fi
        echo "An update is available."
        exit 1
      fi
      echo "Plainwire is not installed locally."
      echo "Latest release: $latest_no_v"
      exit 1
      ;;
    update)
      if [[ -x "$INSTALL_BIN" ]] && [[ -z "$cur" ]]; then
        die "There is already a program at $INSTALL_BIN that is not Plainwire. Run '$0' (install mode) to replace it."
      fi
      [[ -n "$cur" ]] || die "Plainwire is not installed yet. Run '$0' (install mode) first."
      if [[ "$cur" == "$latest_no_v" ]]; then
        install_new_version "$latest_no_v" reinstall
      else
        install_new_version "$latest_no_v" update
      fi
      ;;
    install)
      if [[ -x "$INSTALL_BIN" ]]; then
        if [[ -z "$cur" ]]; then
          echo "==> Found a non-Plainwire program at $INSTALL_BIN; it will be replaced."
          install_new_version "$latest_no_v" replace
        elif [[ "$cur" == "$latest_no_v" ]]; then
          install_new_version "$latest_no_v" reinstall
        else
          echo "==> Found an existing install ($cur); I will update it to $latest_no_v."
          install_new_version "$latest_no_v" update
        fi
      else
        install_new_version "$latest_no_v" install
      fi
      ;;
  esac
}

main "$@"