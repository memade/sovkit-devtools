# Notices

SovKit DevTools source follows the root MIT LICENSE.

`3rdparty/libwxui` was supplied by the project owner from OrbitBridge. It is covered by the upstream MIT license (Copyright 2026 memade), included at `3rdparty/libwxui/LICENSE`. Existing source attribution is retained. This repository supplies a standalone build target and omits unused generated installation/pkg-config files from the snapshot.

The tool uses wxWidgets (wxWindows Library Licence), fmt (MIT), RapidJSON (MIT and its bundled notices), utfcpp (Boost Software License), and nlohmann/json (MIT). Their exact source and copyright notices are provided by the pinned vcpkg recipes; vcpkg builds copy installed dependency copyright files into packages.

The UI bundles Noto Sans CJK SC and Noto Sans Mono CJK SC under the SIL Open Font License 1.1. The unmodified fonts, license, upstream paths and checksums are in `res/fonts`. Text editing uses wxWidgets' Scintilla component; its notices are included by the wxWidgets vcpkg recipe.

SovKit is separately licensed. SDK packages must retain their own LICENSE, NOTICE.md, third-party notices and covered source obligations. Supplying a shared library does not change those terms. DevTools does not implement, copy or statically link SDK internals.
