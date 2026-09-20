# SovKit DevTools

**Explore and debug device-to-device communication with SovKit.**

English | [简体中文](README.zh-CN.md) · [SovKit website](https://skstu.com) · [Report an issue](https://github.com/memade/sovkit-devtools/issues) · [Contribute](CONTRIBUTING.md)

SovKit DevTools is an open-source desktop workbench for developers integrating the SovKit SDK. Load the library, create an isolated test identity, inspect requests and events, and reproduce pairing, messaging, and file-transfer problems. A companion CLI lets you automate the same workflows.

**0.1.0 is an initial preview.** macOS arm64 and Windows x64 have native builds, GUI startup checks and real SDK loopback regressions. Cross-machine/high-DPI manual testing remains separate; Linux has presets but is not yet validated. The current GUI is primarily in Chinese.

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
| Windows x64 | MSVC Debug GUI/CLI built; public SDK and Unicode loopback regressions passed | High DPI, two-machine UI workflows, distribution |
| Linux x64 / arm64 | Build presets available; not yet validated on Linux | Native builds, GTK integration, networking, and packaging |

Tests cover pairing confirmation, message delivery, exact file bytes, encrypted profile reopening, exclusive profile access, and wrong-password data retention. These are local tests, not a claim of complete cross-platform interoperability. See the [validation record (中文)](docs/VALIDATION_20260917.md).

## Get started

### 1. Obtain a matching SDK package

All platforms default to the SovKit-supplied package in [`3rdparty/sovkit_sdk/0.1.0`](3rdparty/sovkit_sdk/0.1.0). It currently contains the Windows DLL, macOS arm64 dylib, public header and [SDK integration guide](3rdparty/sovkit_sdk/0.1.0/SDK_INTEGRATION.md). Add the matching Linux library to the same directory when supplied. DevTools consumes these files unchanged; SovKit defines SDK behavior, compatibility and licensing. UI request templates are examples, not a separate SDK specification. DevTools neither builds nor downloads the SDK.

Use a 0.1.0 SDK with ABI 25 and `sovkit_info().keystoreDetachVersion >= 1`. A library without this capability can be inspected, but test identities will not start.

```text
3rdparty/sovkit_sdk/0.1.0/
  sovkit.h
  libsovkit.dll         # Supplied Windows library
  libsovkit.dylib       # Supplied macOS arm64 library
  libsovkit.so          # Linux: add the supplied library before building
  SDK_INTEGRATION.md    # SovKit's authoritative integration guide
```

An explicitly selected SovKit package may also use the documented `include/`, `lib/` or `bin/` layout with its integration guide named `README.md`. Headers, runtime and documentation must come from the same supplied package.

### 2. Build

Install CMake 3.25+, Ninja, Python 3, a C++20 toolchain, and [vcpkg](https://github.com/microsoft/vcpkg). The manifest pins the dependency baseline.

```sh
git clone https://github.com/memade/sovkit-devtools.git
cd sovkit-devtools
export VCPKG_ROOT=/path/to/vcpkg
```

Use the same configure/build pairs as OrbitBridge. CMake selects `libsovkit.dll`, `libsovkit.dylib` or `libsovkit.so` from the default package for the target platform and copies it beside the GUI executable. Startup loads that copy using the executable's directory, independently of the working directory. On macOS both files reside in `sovkit-devtools.app/Contents/MacOS/`. Deleting `.build` does not lose the default SDK selection. Python is still needed to generate embedded resources and API bindings.

#### macOS

The supplied arm64 library is selected automatically. CMake also selects the system SDK from the active Xcode/Command Line Tools installation before vcpkg probes its compiler. No manual `SDKROOT` export is needed; an explicit `CMAKE_OSX_SYSROOT` (including a cached selection) takes precedence over `SDKROOT`.

```sh
cmake --preset macos-arm64-debug
cmake --build --preset macos-arm64-debug
ctest --preset macos-arm64-debug

cmake --preset macos-arm64-release
cmake --build --preset macos-arm64-release
```

Intel Macs use `macos-x64-debug` / `macos-x64-release`, with an x64 SDK. Build an individual target with `cmake --build --preset macos-arm64-debug --target sovkit-devtools` or `--target sovkit-console`. Open `.build/macos-arm64-debug/sovkit-devtools.app`.

#### Linux

Install the compiler and wxGTK development dependencies first; for Debian/Ubuntu these typically include `build-essential ninja-build pkg-config libgtk-3-dev libx11-dev libgl1-mesa-dev libglu1-mesa-dev`. Place SovKit's matching Linux library at `3rdparty/sovkit_sdk/0.1.0/libsovkit.so`, then use:

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug

cmake --preset linux-release
cmake --build --preset linux-release
```

`linux-debug` / `linux-release` target x64, matching OrbitBridge's names. The existing `linux-x64-debug` / `linux-x64-release` names remain available; each preset has its own build directory and SDK cache. ARM64 Linux uses `linux-arm64-debug` / `linux-arm64-release` with an arm64 SDK. GUI tests require a graphical session.

List presets with `cmake --list-presets=all`. The optional `scripts/build.py <preset> --sdk /path/sdk --test` wrapper remains available for Ninja presets. For a local macOS Release ZIP, build Release and run `cpack --config .build/macos-arm64-release/CPackConfig.cmake -B .build/packages`.

#### Windows

On Windows, use **Visual Studio 2026** with Desktop development with C++, CMake 4.2+, Python 3.8+, and vcpkg. CMake and Python must be on PATH. Set the `VCPKG_ROOT` user environment variable in Windows, then reopen applications. The supplied SDK is selected by default. Double-click [`scripts/build-vs2026.bat`](scripts/build-vs2026.bat) to generate and open `.build/windows-vs2026-x64/sovkit-devtools.slnx`. Select **Debug / x64**, build the solution, set breakpoints, and press **F5**. If needed, right-click `sovkit-devtools` and select **Set as Startup Project**. Older CMake versions may generate `.sln` instead; the script recognizes both formats. This workflow does not require Ninja or PowerShell.

The generated Visual Studio solution separates applications, static libraries, tests and build helpers:

```text
3rdparty/
  libwxui
  sovkit-sdk                 # Browse the supplied header and integration guide
projects/
  sovkit-devtools             # Default startup project
  sovkit-console
libraries/
  devtools_core
tests/
  devtools-core-tests
  libwxui-desktop-tests
CMakeTargets/
  ALL_BUILD, ZERO_CHECK, INSTALL, PACKAGE, RUN_TESTS
  staging/                   # SDK runtime and optional symbols
  CTestDashboard/
```

Project filters mirror `src`, `include`, `res`, `docs`, `scripts` and `cmake` where applicable; generated headers have a separate `generated` filter. The GUI project also exposes the root build settings and VS Code configuration. These are IDE groups, not relocated files. After updating CMake, run `cmake --preset windows-debug` and reload the solution when Visual Studio prompts. The layout is maintained in `cmake/IdeLayout.cmake`; do not edit generated `.slnx`, `.vcxproj` or `.filters` files. Console-only builds use `sovkit-console` as the startup project.

The script falls back to `%USERPROFILE%/vcpkg` when `VCPKG_ROOT` is unset. It preserves existing build directories. Alternatively, use ordinary **CMD** from the repository root, following the same preset workflow as OrbitBridge:

```bat
set "VCPKG_ROOT=C:\path\to\vcpkg"
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug

cmake --preset windows-release
cmake --build --preset windows-release
```

CMake defaults to `3rdparty/sovkit_sdk/0.1.0` and migrates the former built-in `.sdk/windows-x64-current` cache path. To select a different SovKit-supplied package, configure with `-DSOVKIT_SDK_ROOT=C:/other/sdk` or use `scripts/build.py --sdk /path/sdk`. The environment variable of the same name no longer overrides this default. The build cache retains an explicit selection.

Build individual targets with `cmake --build --preset windows-debug --target sovkit-devtools` or `--target sovkit-console`. Both configurations share the `.build/windows-vs2026-x64/` solution; executables go to `.build/windows-vs2026-x64/Debug/` or `.build/windows-vs2026-x64/Release/`. GUI builds copy the selected SDK DLL unchanged, plus a matching PDB only if SovKit supplied one. Debug/Release changes only the DevTools build; the supplied SDK remains unchanged. Stop the application before replacing its SDK.

The existing `windows-x64-debug` / `windows-x64-release` Ninja presets also support the configure/build/test commands above or `scripts/build.py` from an x64 developer shell. Use CMake directly or Visual Studio for the `windows-debug` / `windows-release` presets, not `build.py`.

All platforms keep build outputs under `.build/`. Ninja uses `.build/<preset>/`; Visual Studio uses one `.build/windows-vs2026-x64/` solution with `<configuration>/` subdirectories so Debug and Release can be switched in the IDE. Reconfigure with the updated preset to create this directory; CMake caches from the former `out/` directory cannot be moved here. Windows/Linux produce a `sovkit-devtools` executable with the platform's usual extension. SDK binaries must match the target OS and architecture; an Android `.so` is not a Linux desktop SDK.

For existing dependency installations, configure with `-DSOVKIT_SDK_ROOT=/path/sdk` and `-DCMAKE_PREFIX_PATH=/path/dependencies`. Set `-DDEVTOOLS_BUILD_GUI=OFF` to build only the console and integration tests. Local packages are not a signed or notarized public release.

### Windows distribution

The GUI ZIP contains only `sovkit-devtools.exe` and `libsovkit.dll`. XML, images,
SDK documentation and license notices are embedded by `scripts/embed_assets.py`;
SDK documentation and Ab.build/windows-vs2026-x64/licenses can be opened inside the application.
Windows uses installed Microsoft YaHei; macOS/Linux use native system fonts.
No font or resource directory is required. The console remains a build/test target.

```bat
cmake --build --preset windows-release
cpack --config .build/windows-vs2026-x64/CPackConfig.cmake -C Release -B .build/packages
```

Distribute the ZIP, not the development output directory, which may contain PDBs,
test executables or old build artifacts.

### 3. Run a two-device experiment

1. Startup automatically loads the platform SDK beside the executable (`libsovkit.dll`, `libsovkit.dylib` or `libsovkit.so`) and queries its capabilities, independently of the working directory. If absent, use the SDK picker. Loading does not start an identity or network operation. On each computer, choose a different device name. Leave the profile directory empty for a temporary identity, or choose a separate empty directory and password for persistence.
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

The next milestone is **cross-machine usability and Linux validation**: high DPI, two-machine communication, failure recovery, and packaging. Native Bluetooth GATT integration is separate future work. WAN/network-sharing expansion is deferred.

Useful contributions include small fixes, reproducible issues, platform test results, and documentation improvements. See [CONTRIBUTING.md](CONTRIBUTING.md), the [manual test checklist (中文)](docs/MANUAL_TESTING.md), and [architecture notes (中文)](docs/ARCHITECTURE.md). Changes are recorded in [CHANGELOG.md](CHANGELOG.md).

If this helps your integration, star the repository, share your findings, and explore the SDK at [skstu.com](https://skstu.com).

## License and acknowledgments

DevTools-owned code is released under the [MIT License](LICENSE). The bundled libwxui snapshot retains its [upstream MIT License](3rdparty/libwxui/LICENSE) and attribution. The desktop approach was inspired by OrbitBridge/brostu: C++20, wxWidgets, libwxui, CMake, and vcpkg, with independent build integration here.

**The SovKit SDK is separately licensed.** DevTools' MIT license does not relicense SDK binaries or their dependencies. Preserve the licensing materials supplied with each SDK package. See [NOTICE.md](NOTICE.md) for dependency attribution.


### Windows SDK debugging

Use Python 3.8+ and an x64 Developer PowerShell. The standalone SDK needs
`include/sovkit.h`, `README.md` (integration guide), and `bin/libsovkit.dll`
(or `lib/libsovkit.dll`; `lib/` takes precedence). No SDK import library is needed.
Run `python scripts/build.py windows-x64-debug --sdk C:/SDK --test`, then open
`.build/windows-x64-debug/sovkit-devtools.exe`. Select the library, load it, run
`selftest`, then start a disposable identity. Restart the tool to change SDKs.
Windows loader errors include the OS code (126: dependency; 193: architecture).
The CLI accepts Unicode DLL paths and UTF-8 JSONL. The two-process regression
uses loopback, not a cross-machine or Bluetooth radio test.
