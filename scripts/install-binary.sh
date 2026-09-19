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
  local msg="$1" ans
  if [[ "$ASSUME_YES" -eq 1 ]]; then
    return 0
  fi
  if [[ "$INTERACTIVE" -eq 0 ]]; then
    return 1
  fi
  read -rp "$msg " ans
  case "${ans}" in
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

local_version() {
  "$INSTALL_BIN" --version 2>/dev/null | tail -n1 | grep -oE '[0-9]+(\.[0-9]+)+' | head -n1 || true
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

maybe_add_to_path() {
  case ":$PATH:" in
    *":$PREFIX/bin:"*) return 0 ;;
  esac

  if [[ "$INTERACTIVE" -eq 1 ]] && prompt_yn "Add $PREFIX/bin to your PATH? [y/N]"; then
    local shell_name rc line
    shell_name="$(basename "${SHELL:-/bin/sh}")"
    case "$shell_name" in
      bash) rc="$HOME/.bashrc" ;;
      zsh) rc="$HOME/.zshrc" ;;
      *) rc="$HOME/.profile" ;;
    esac
    line="export PATH=\"$PREFIX/bin:\$PATH\""
    if [[ -f "$rc" ]] && grep -qF "$PREFIX/bin" "$rc"; then
      echo "==> PATH entry already present in $rc, nothing to do."
    else
      printf '\n# Add Plainwire bin directory to PATH\n%s\n' "$line" >> "$rc"
      echo "==> Added to $rc. Run 'source $rc' (or restart your terminal) to pick it up."
    fi
  else
    echo "==> Run Plainwire with: $INSTALL_BIN"
    echo "    (or add $PREFIX/bin to your PATH to use 'plainwire-desktop')."
  fi
}

install_new_version() {
  local new_ver="$1" mode="$2"

  if [[ "$mode" == "update" ]]; then
    echo "==> Found current version: $(local_version)"
    if ! prompt_yn "Update to Plainwire $new_ver? [y/N]"; then
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
  rm -f "$BACKUP_BIN"
  BACKUP_RESTORED=1
  refresh_desktop_db

  if [[ "$mode" == "update" ]]; then
    echo "==> Done: Plainwire updated to $new_ver."
  else
    echo "==> Done: installed Plainwire $new_ver to $PREFIX."
  fi
  echo "==> Launch it with: plainwire-desktop"
  if [[ "$mode" == "install" ]]; then
    maybe_add_to_path
  fi
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
  if [[ -x "$INSTALL_BIN" ]]; then
    cur="$(local_version)"
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
      [[ -n "$cur" ]] || die "Plainwire is not installed yet. Run '$0' (install mode) first."
      if [[ "$cur" == "$latest_no_v" ]]; then
        echo "==> Already up to date ($cur). Nothing to do."
        exit 0
      fi
      install_new_version "$latest_no_v" update
      ;;
    install)
      if [[ -n "$cur" ]]; then
        if [[ "$cur" == "$latest_no_v" ]]; then
          echo "==> Plainwire $cur is already installed. Nothing to do."
          exit 0
        fi
        echo "==> Found an existing install ($cur); I will update it to $latest_no_v."
        install_new_version "$latest_no_v" update
      else
        install_new_version "$latest_no_v" install
      fi
      ;;
  esac
}

main "$@"