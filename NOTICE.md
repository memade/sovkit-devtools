# Notices

SovKit DevTools source follows the root MIT LICENSE.

`3rdparty/libwxui` was supplied by the project owner from OrbitBridge. It is covered by the upstream MIT license (Copyright 2026 memade), included at `3rdparty/libwxui/LICENSE`. Existing source attribution is retained. This repository supplies a standalone build target and omits unused generated installation/pkg-config files from the snapshot.

The tool uses wxWidgets (wxWindows Library Licence), fmt (MIT), RapidJSON (MIT and its bundled notices), utfcpp (Boost Software License), and nlohmann/json (MIT). Their exact source and copyright notices are provided by the pinned vcpkg recipes; installed dependency copyright files are embedded into the application and accessible through About and licenses.

The UI uses system-installed fonts: Microsoft YaHei on Windows and native system defaults on macOS/Linux. No font files are bundled or registered. Text editing uses wxWidgets' Scintilla component; its notices are included by the wxWidgets vcpkg recipe.

SovKit is separately licensed. SDK packages must retain their own LICENSE, NOTICE.md, third-party notices and covered source obligations. Supplying a shared library does not change those terms. DevTools does not implement, copy or statically link SDK internals.

`3rdparty/sovkit_sdk/0.1.0` contains the SDK artifacts supplied by the project owner. Its header, library and `SDK_INTEGRATION.md` are maintained by SovKit and consumed unchanged. SDK behavior, compatibility, error semantics and licensing are defined by SovKit; DevTools request templates are usage examples, not an independent SDK specification. The root MIT license does not cover these SDK artifacts.
