#!/usr/bin/env bash
set -euo pipefail

# Plainwire Desktop - updater
#
# Checks whether a newer prebuilt binary is available and, if you accept the
# prompt, updates your install atomically (checksum + backup + rollback), so
# it can never be left half-updated.
#
# Can be curl-piped straight from the repo or run from a local checkout:
#   curl -fsSL https://raw.githubusercontent.com/Plainwire-development/Plainwire-desktop/main/scripts/update.sh | bash

REPO="${PLAINWIRE_REPO:-Plainwire-development/Plainwire-desktop}"

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" 2>/dev/null && pwd || true)"

if [[ -n "$SCRIPT_DIR" ]] && [[ -x "$SCRIPT_DIR/install-binary.sh" ]]; then
  exec bash "$SCRIPT_DIR/install-binary.sh" update "$@"
fi

echo "==> Downloading the Plainwire installer ..."
TMP="$(mktemp)"
trap 'rm -f "$TMP"' EXIT

curl -fsSL "https://raw.githubusercontent.com/$REPO/main/scripts/install-binary.sh" -o "$TMP" || {
  echo "ERROR: could not fetch the installer from $REPO" >&2
  exit 1
}

bash "$TMP" update "$@"