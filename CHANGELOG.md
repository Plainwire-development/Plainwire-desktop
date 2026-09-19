# Changelog

## 2.4.1

- fixed the desktop polish stylesheet not loading in 2.4.0: the origin placeholder and the CSS both used `%1`, so the injected script became invalid JavaScript (`Unexpected token ':'`) and the polished desktop UI was never applied;
- the prebuilt installer now writes the menu entry with the absolute install path in `Exec`, so the app launches from the desktop/application menu even though GUI sessions do not put `~/.local/bin` on `PATH`;
- the installer also removes a stale `plainwire-desktop-handler.desktop` that pointed at a nonexistent `/usr/bin/plainwire-desktop` and breaks `plainwire://` links, and rebuilds the KDE menu cache after installing.

## 2.4.0

- added a Servers tab so Plainwire Desktop can connect to self-hosted Plainwire servers instead of only the official `https://plainwi.re`;
- servers are validated, saved per user and switchable from the app, the system tray and the `Ctrl+Shift+S` shortcut;
- deep links, permission prompts, notifications and navigation policies now follow the active server;
- added a reset option to return to the official server at any time;
- fixed the prebuilt installer silently cancelling when piped through `curl … | bash` (it now prompts on the controlling terminal);
- made the installer's Qt runtime check also probe the standard system library directories so it doesn't warn on Debian when ldconfig's cache is stale.

## 2.3.0

- moved the production service to `https://plainwi.re`;
- added a desktop polish layer for better scaling, settings layout, message controls and reaction hit targets without forking Plainwire's frontend state;
- replaced JavaScript `alert()` popups from Plainwire with a non-blocking in-window notice;
- added persistent zoom controls and useful desktop shortcuts;
- added explicit first-use permission prompts for microphone, camera, notifications and screen capture instead of silently granting them;
- improved renderer crash recovery and download completion feedback;
- kept the message viewport free of expensive blur and animation effects so the Chromium compositor stays smooth;
- simplified the README and made it user-facing.

## 2.2.0

- made Qt 6 WebEngine/Chromium the only desktop renderer;
- removed the Tauri/WebKitGTK implementation, Rust/Cargo files, Node/Tauri tooling, bundled WebKit UI shell, updater-specific Tauri workflows and WebKit compatibility modes;
- promoted the Qt project from `linux-qt/` to the repository root;
- split the Qt client into focused source files for navigation policy, window/profile behavior and single-instance handling;
- removed committed build output and generated CMake/Ninja files;
- kept persistent Plainwire sessions, localStorage/cache/permissions, calls, capture, downloads, notifications, tray behavior, deep links and window state;
- bounded single-instance IPC payloads and retained per-user local-server access plus stale-socket recovery;
- replaced Node-based release verification with a small shell source checker;
- simplified CI/release jobs to the one supported Qt renderer.

## 2.1.2

- fixed Qt 6.10 compilation on Debian Sid;
- fixed the single-instance callback capture and Qt container index types;
- added warning-as-error Qt CI coverage.

## 2.1.1

- disabled Ninja's automatic CMake regeneration because the build wrapper already configures explicitly, avoiding dirty-manifest loops on skewed mtimes.

## 2.1.0

- introduced the Qt 6 WebEngine/Chromium renderer with persistent browser state, tray integration, capture permissions, downloads, notifications and deep links.
