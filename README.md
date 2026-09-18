# Plainwire Desktop

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

## Build

On Debian:

```sh
sudo apt install \
  build-essential cmake ninja-build \
  qt6-base-dev qt6-webengine-dev qt6-wayland

./build.sh
./build/plainwire-desktop
```

Install it for your user with:

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

This is license under our PlainSimple 1.0 license. see [LICENSE](https://github.com/Plainwire-development/Plainwire-desktop/blob/main/LICENSE) for details.
