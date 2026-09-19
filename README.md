# Plainwire Desktop
# > currently it has wiring for debian systems. will add more interactive support for others.

Plainwire Desktop is the native desktop app for [plainwi.re](https://plainwi.re).

It uses Qt 6 and QtWebEngine, so the app runs the same Plainwire client and account as the website while keeping desktop things like notifications, downloads, screen sharing, deep links, tray behavior, and window state native.

## What works

- messages, DMs, servers, reactions, uploads, calls and presence
- persistent login and app settings
- microphone, camera and screen sharing
- desktop notifications
- downloads with a native save dialog
- `plainwire://` links
- close to tray
- saved window size, position and zoom
- external links open in your normal browser

The desktop app uses QtWebEngine/Chromium. There is no Tauri/WebKitGTK version anymore.

## Install the prebuilt binary (fastest)

Every tagged release ships prebuilt binaries for Linux **x86_64** and **arm64**.
This one-liner detects your platform, downloads the right binary, verifies its
SHA-256 checksum, sanity-runs it, and installs it to `~/.local`:

```sh
curl -fsSL https://raw.githubusercontent.com/Plainwire-development/Plainwire-desktop/main/scripts/install-binary.sh | bash
```

The replacement is atomic: your current binary is backed up, and if anything
fails it is rolled back, so the app is never left in a broken state.

Prebuilt binaries are available for Linux only. On other platforms, or if a
binary for your CPU is not available, build from source (below); the installer
tells you when that is the case.

## Update

To check for and apply a newer version (it prompts first, and does nothing if
you are already on the latest):

```sh
curl -fsSL https://raw.githubusercontent.com/Plainwire-development/Plainwire-desktop/main/scripts/update.sh | bash
```

Or, from a local checkout: `./scripts/update.sh`

Settings, logins, downloads and window state live in `~/.config` (not in the
binary), so updating never touches your data.

## Build from source

On Debian:

```sh
sudo apt install \
  build-essential cmake ninja-build \
  qt6-base-dev qt6-webengine-dev qt6-wayland

./build.sh
./build/plainwire-desktop
```

Install the source build for your user with:

```sh
./install-local.sh
```

The default install location is `~/.local`.

## Development

Run the source checks with:

```sh
./scripts/verify.sh
```

The CI build compiles against Debian Sid Qt WebEngine with compiler warnings treated as errors.

# Licensing

This is license under our PlainSimple 1.0 license.

> see [LICENSE](https://github.com/Plainwire-development/Plainwire-desktop/blob/main/PLAINWIRE-LICENSE) for details.
