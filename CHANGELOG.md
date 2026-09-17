# Changelog

## Unreleased

- Create private Windows profiles with the current user as owner and an inheritable user/SYSTEM ACL; fix persistent store open failures.

- Fix Windows Python 3.8 API generation, UTF-8 source reading, Unicode DLL paths and console encoding.
- Accept Windows bin/ SDK layouts; include OS loader diagnostics.
- Select the receive API when choosing a destination; correct recovery and store-result templates.
- Extend real SDK regressions to Unicode DLL/profile/file paths and messages.
- Refresh the embedded SDK integration guide.

## 0.1.0 — Initial source preview — 2026-09-16

- Native wxWidgets/libwxui workbench and JSONL console consuming only the public SovKit SDK package.
- Typed dynamic loading of 84 C exports, ABI/capability checks, and 58 JSON operations.
- Isolated test identities, encrypted persistent profiles, LAN pairing, messages, files, events, and logs.
- Metadata-only GUI diagnostic export and two-process communication regression tests.
- CMake/vcpkg build presets for macOS, Windows, and Linux.
- macOS arm64 Debug/Release and extracted-package validation completed; Windows/Linux validation remains pending.
- BLE frame debugging only; no native desktop GATT bridge. WAN/network-sharing validation is deferred.

This is an initial source preview, not a claim of signed binary releases or complete cross-platform validation. SDK binaries are obtained separately.
