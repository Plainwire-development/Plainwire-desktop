#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

fail() {
  printf 'verify: %s\n' "$*" >&2
  exit 1
}

[[ -f CMakeLists.txt ]] || fail 'missing CMakeLists.txt'
[[ -f src/main.cpp ]] || fail 'missing Qt source'
[[ -f src/main_window.cpp ]] || fail 'missing main window source'
[[ -f src/plainwire_page.cpp ]] || fail 'missing web page source'
[[ -f resources/plainwire.png ]] || fail 'missing app icon'
[[ -f resources/desktop-polish.css ]] || fail 'missing desktop polish stylesheet'
[[ -f tests/url_policy_test.cpp ]] || fail 'missing URL policy regression tests'
[[ -f packaging/me.kokonico.plainwire.desktop ]] || fail 'missing desktop entry'

bash -n build.sh install-local.sh scripts/verify.sh

for legacy in src-tauri package.json package-lock.json ui linux-qt; do
  [[ ! -e "$legacy" ]] || fail "legacy renderer/tooling remains: $legacy"
done

if grep -RniE --exclude-dir=.git --exclude='CHANGELOG.md' --exclude='README.md' \
  '(webkitgtk|src-tauri|tauri::|@tauri-apps|WEBKIT_DISABLE_)' \
  CMakeLists.txt src build.sh install-local.sh .github resources tests 2>/dev/null; then
  fail 'legacy WebKit/Tauri references remain in active project files'
fi

grep -qF 'project(PlainwireDesktop VERSION 2.4.0 LANGUAGES CXX)' CMakeLists.txt \
  || fail 'CMake version is not 2.4.0'
grep -qF 'https://plainwi.re' src/app_config.hpp \
  || fail 'production Plainwire URL is not plainwi.re'
grep -qF 'plainwi.re' src/main.cpp \
  || fail 'organization domain is not plainwi.re'

if grep -RniF 'plainwire.kokonico.me' src resources CMakeLists.txt build.sh install-local.sh packaging \
  --exclude='url_policy_test.cpp' 2>/dev/null; then
  fail 'legacy production URL remains in active runtime files'
fi

for needle in \
  'QWebEngineProfile' \
  'ForcePersistentCookies' \
  'PersistentPermissionsPolicy::StoreOnDisk' \
  'Accelerated2dCanvasEnabled' \
  'ScreenCaptureEnabled' \
  'QSystemTrayIcon' \
  'QLocalServer' \
  "shell: 'qtwebengine'" \
  "engine: 'chromium'" \
  'plainwire-desktop-polish' \
  'renderProcessTerminated' \
  'setZoomFactor' \
  'permissionDescription' \
  'HighDpiScaleFactorRoundingPolicy::PassThrough'; do
  grep -RqF "$needle" src || fail "missing Qt invariant: $needle"
done

grep -qF 'javaScriptAlert' src/plainwire_page.cpp \
  || fail 'blocking JavaScript alert override is missing'
grep -qF "classList.add('plainwire-desktop')" src/main_window.cpp \
  || fail 'desktop document marker is missing'
grep -qF 'rawImageData' src/main_window.cpp \
  || fail 'raw image-data presentation guard is missing'
grep -qF '.message-reaction' resources/desktop-polish.css \
  || fail 'reaction desktop styling is missing'
grep -qF '.settings-page' resources/desktop-polish.css \
  || fail 'responsive settings desktop styling is missing'
grep -qF 'backdrop-filter: none !important' resources/desktop-polish.css \
  || fail 'desktop hot-path blur guard is missing'
grep -qF 'prefers-reduced-motion' resources/desktop-polish.css \
  || fail 'reduced-motion styling is missing'

grep -qF 'plainwire-url-policy-tests' CMakeLists.txt \
  || fail 'URL policy test target is missing'
grep -qF 'ctest --test-dir /tmp/plainwire-build --output-on-failure' .github/workflows/check.yml \
  || fail 'CI does not run URL policy tests'
grep -qF 'x-scheme-handler/plainwire' packaging/me.kokonico.plainwire.desktop \
  || fail 'desktop entry does not register plainwire://'
grep -qF 'set(CMAKE_SUPPRESS_REGENERATION ON)' CMakeLists.txt \
  || fail 'CMake regeneration guard missing'
grep -qF -- '-DCMAKE_SUPPRESS_REGENERATION=ON' build.sh \
  || fail 'build wrapper regeneration guard missing'

if grep -RniE --exclude-dir=.git --exclude='CHANGELOG.md' --exclude='*.png' \
  '\b(TODO|FIXME|HACK|XXX)\b' src resources tests CMakeLists.txt 2>/dev/null; then
  fail 'unfinished source marker found'
fi

if find . -path './build' -prune -o \
  \( -name '*.o' -o -name '*.a' -o -name '*.so' -o -name '*.dylib' -o -name '*.dll' -o -name 'CMakeCache.txt' \) \
  -print | grep -q .; then
  fail 'compiled/generated artifacts found in source tree'
fi

printf 'Plainwire Desktop 2.4.0 source checks passed.\n'
