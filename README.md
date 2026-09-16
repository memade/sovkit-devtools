# SovKit DevTools

**Explore and debug device-to-device communication with SovKit.**

English | [简体中文](README.zh-CN.md) · [SovKit website](https://skstu.com) · [Report an issue](https://github.com/memade/sovkit-devtools/issues) · [Contribute](CONTRIBUTING.md)

SovKit DevTools is an open-source desktop workbench for developers integrating the SovKit SDK. Load the library, create an isolated test identity, inspect requests and events, and reproduce pairing, messaging, and file-transfer problems. A companion CLI lets you automate the same workflows.

**0.1.0 is an initial preview.** macOS arm64 has been built and exercised. Windows and Linux have build presets; validation and polish on those systems are the next priority. The current GUI is primarily in Chinese.

## Why this project exists

Communication bugs are easier to investigate when both ends are visible. This workbench gives SDK integrators a place to inspect return codes, follow events, compare pairing safety codes, and reproduce failures independently of a product application.

DevTools consumes just **`libsovkit` + `sovkit.h` + the SDK integration guide**. It does not require SovKit implementation sources, Flutter, or OrbitBridge sources. It uses the public C ABI through `dlopen` / `LoadLibraryExW`; Windows does not require an SDK import `.lib`.

[SovKit](https://skstu.com) provides the underlying communication SDK. DevTools provides the public integration example and a place to contribute reproducible feedback.

## What you can try

- **Inspect an SDK:** check ABI, capabilities, exported symbols, and a binary encryption round trip.
- **Manage test identities:** use memory-only sessions or password-protected, isolated profiles backed by the SDK vault and SQLCipher.
- **Exercise LAN workflows:** discovery, pairing with safety-code confirmation on both peers, relationships, messages, and file transfers.
- **See what happened:** editable JSON templates, raw responses, return codes, timings, events, and logs.
- **Automate reproduction:** a JSONL console and tests using two real OS processes.
- **Share limited diagnostics:** the GUI export includes allowlisted metadata; it excludes request bodies, identity material, messages, and paths. Raw responses and console output may contain private data.

The current adapter imports 84 public C symbols and exposes 58 JSON operations. Lifecycle, callbacks, and raw key pointers are managed by the host.

BLE support currently means **SDK frame injection/polling**, not OS Bluetooth scanning or GATT connections. WAN/network-sharing operations remain manual experiments and are outside the first preview's validation focus.

## Platform status

| Platform | Current evidence | Next step |
| --- | --- | --- |
| macOS arm64 | Debug/Release builds, GUI launch, extracted ZIP self-test, and two-process LAN tests passed | Broader two-machine testing and distribution polish |
| macOS x64 | Build preset available | Build and test on matching hardware |
| Windows x64 | MSVC/vcpkg presets available; not yet validated on Windows | Native builds, paths, UI, networking, and packaging |
| Linux x64 / arm64 | Build presets available; not yet validated on Linux | Native builds, GTK integration, networking, and packaging |

Tests cover pairing confirmation, message delivery, exact file bytes, encrypted profile reopening, exclusive profile access, and wrong-password data retention. These are local tests, not a claim of complete cross-platform interoperability. See the [validation record (中文)](docs/VALIDATION_20260916.md).

## Get started

### 1. Obtain a matching SDK package

**This source repository does not bundle or automatically download SDK binaries.** Visit the [SovKit website](https://skstu.com) for the project, or [ask the maintainer in an issue](https://github.com/memade/sovkit-devtools/issues/new) about SDK package availability for your OS and architecture. A GitHub source archive is not a ready-to-run application.

Use a 0.1.0 SDK with ABI 25 and `sovkit_info().keystoreDetachVersion >= 1`. A library without this capability can be inspected, but test identities will not start.

```text
sdk/
  include/sovkit.h
  lib/libsovkit.dylib   # Windows: libsovkit.dll; Linux: libsovkit.so
  README.md            # SDK integration guide
  LICENSE / NOTICE.md / licenses/ / third_party/  # as supplied by the SDK
```

### 2. Build

Install CMake 3.25+, Ninja, Python 3, a C++20 toolchain, and [vcpkg](https://github.com/microsoft/vcpkg). The manifest pins the dependency baseline.

```sh
git clone https://github.com/memade/sovkit-devtools.git
cd sovkit-devtools
export VCPKG_ROOT=/path/to/vcpkg
python3 scripts/build.py macos-arm64-debug --sdk /absolute/path/sdk --test
```

Open `.build/macos-arm64-debug/sovkit-devtools.app` on macOS. Use `macos-arm64-release --test --package` for a local Release ZIP.

On Windows, run in an **x64 Developer PowerShell**:

```powershell
$env:VCPKG_ROOT = "C:/tools/vcpkg"
python scripts/build.py windows-x64-debug --sdk C:/SDK --test
```

On Linux, use `linux-x64-debug` or `linux-arm64-debug`. Install the compiler and wxGTK development dependencies first; for Debian/Ubuntu these typically include `build-essential ninja-build pkg-config libgtk-3-dev libx11-dev libgl1-mesa-dev libglu1-mesa-dev`.

Build outputs go under `.build/<preset>/`. Windows/Linux produce a `sovkit-devtools` executable with the platform's usual extension. SDK binaries must match the target OS and architecture; an Android `.so` is not a Linux desktop SDK.

For existing dependency installations, configure with `-DSOVKIT_SDK_ROOT=/path/sdk` and `-DCMAKE_PREFIX_PATH=/path/dependencies`. Set `-DDEVTOOLS_BUILD_GUI=OFF` to build only the console and integration tests. Local packages are not a signed or notarized public release.

### 3. Run a two-device experiment

1. On each computer, load its platform SDK and choose a different device name. Leave the profile directory empty for a temporary identity, or choose a separate empty directory and password for persistence.
2. Start the identity, then run `discovery_start` and `discovery_list`.
3. Copy a candidate's `address` and `pairingPort` into `pairing_start`. Inspect `pairing_status` on both sides, compare `safetyCode`, and run `pairing_confirm` on both peers.
4. Use the resulting relationship ID in `message_send`. Inspect events on the receiver. For files, select a source file and explicitly accept the transfer into a chosen destination directory on the receiver.
5. Inspect responses and logs. Use Stop to save and end the session; if stopping fails, keep the window open, resolve the reported issue, and retry.

Profiles are dedicated to DevTools. Do not point them at production application data. Test passwords cannot be recovered, and a profile lock rejects simultaneous access by another process.

For scripted experiments, send JSON lines to `sovkit-console /absolute/path/libsovkit.*`:

```json
{"id":1,"op":"info"}
{"id":2,"op":"start","deviceName":"DevTools-A"}
{"id":3,"op":"discovery_start","request":{"candidateTtlMs":12000}}
{"id":4,"op":"discovery_list"}
{"id":5,"op":"stop"}
```

SDK logs may also appear on stdout. Only JSON lines containing `result` are command responses. For persistent profiles, pass `profile` and `password` in the `start` JSON through stdin, not command-line arguments or committed scripts.

## Roadmap and contributions

The next milestone is **Windows/Linux validation and usability**: native builds, Unicode paths, high DPI, two-machine communication, failure recovery, and packaging. Native Bluetooth GATT integration is separate future work. WAN/network-sharing expansion is deferred.

Useful contributions include small fixes, reproducible issues, platform test results, and documentation improvements. See [CONTRIBUTING.md](CONTRIBUTING.md), the [manual test checklist (中文)](docs/MANUAL_TESTING.md), and [architecture notes (中文)](docs/ARCHITECTURE.md). Changes are recorded in [CHANGELOG.md](CHANGELOG.md).

If this helps your integration, star the repository, share your findings, and explore the SDK at [skstu.com](https://skstu.com).

## License and acknowledgments

DevTools-owned code is released under the [MIT License](LICENSE). The bundled libwxui snapshot retains its [upstream MIT License](3rdparty/libwxui/LICENSE) and attribution. The desktop approach was inspired by OrbitBridge/brostu: C++20, wxWidgets, libwxui, CMake, and vcpkg, with independent build integration here.

**The SovKit SDK is separately licensed.** DevTools' MIT license does not relicense SDK binaries or their dependencies. Preserve the licensing materials supplied with each SDK package. See [NOTICE.md](NOTICE.md) for dependency attribution.
