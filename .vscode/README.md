# VS Code 构建与调试

用 VS Code 打开仓库根目录，安装推荐的 C/C++、CMake Tools 扩展；macOS 另需 CodeLLDB。
在“运行和调试”中选择对应平台入口，按 **F5**。启动前会构建项目，SDK 从程序目录自动加载。
“终端 → 运行任务”也可单独执行各平台的 build/test 任务。
各平台构建任务统一先执行 `cmake --preset <name>`，再执行
`cmake --build --preset <name> --parallel 4`；测试任务使用 `ctest --preset <name>`。

## Windows x64

选择 **DevTools · Windows x64 Debug**。任务依次执行 `cmake --preset windows-debug`、
`cmake --build --preset windows-debug --parallel 4`，随后用 MSVC 调试器启动
`.build/windows-vs2026-x64/Debug/sovkit-devtools.exe`。不要求从 Developer PowerShell 启动 VS Code。

需要 Visual Studio 2026 的“使用 C++ 的桌面开发”、CMake 4.2+、Python 3.8+ 和 vcpkg。
设置 `VCPKG_ROOT` 后重新打开 VS Code，确保 `cmake`、`ctest`、`python` 在 PATH 中。
默认 SDK 为 `3rdparty/sovkit_sdk/0.1.0`。现有 CMake 缓存中的显式 SDK 选择会保留。
程序的 PDB 在 `.build/windows-vs2026-x64/Debug`；只有 SovKit 提供匹配 PDB/源码时才能进入 SDK 内部调试。

## Linux x64 / arm64（未实机验证）

在对应架构的 Linux 桌面环境中打开仓库，选择 **DevTools · Linux x64 Debug** 或
**DevTools · Linux arm64 Debug**。这两项是各自平台的本机构建，不是跨架构调试。

- 需要 C++20 编译器、CMake 3.25+、Python 3.8+、Ninja、GDB 和 wxGTK 构建依赖。
  Debian/Ubuntu 可准备 `build-essential ninja-build pkg-config gdb libgtk-3-dev libx11-dev libgl1-mesa-dev libglu1-mesa-dev`。
- 设置 `VCPKG_ROOT`，从该环境启动 VS Code。GDB 默认位置是 `/usr/bin/gdb`；安装在其他位置时修改 `miDebuggerPath`。
- 构建前将 SovKit 提供的匹配架构 `libsovkit.so` 放入 `3rdparty/sovkit_sdk/0.1.0`，
  与随包头文件和接入文档配套使用。构建任务默认使用该目录，不询问路径；工具不会生成或重建 SDK。
- 输出目录为 `.build/linux-x64-debug` 或 `.build/linux-arm64-debug`，调试器为 GDB。
  GUI 需要可用的 X11/Wayland 桌面；普通无显示转发的 SSH 会话不能直接显示窗口。
  使用 VS Code Remote/WSL 时，扩展、编译器、GDB、SDK 和显示环境都需要位于对应远端环境。

## macOS

保留 **DevTools · macOS Debug**，改用仓库公开的 `macos-arm64-debug` 预设，
不再依赖未提交的 `local-macos-debug-current`。默认使用 `3rdparty/sovkit_sdk/0.1.0/libsovkit.dylib`；
应用位于 `.build/macos-arm64-debug/sovkit-devtools.app`。原有 Homebrew PATH 配置保留。
动态库复制到 `.app/Contents/MacOS/`，程序从自身所在目录自动加载它。
设置 `VCPKG_ROOT` 后重新打开 VS Code。CMake 会自动让项目和 vcpkg 使用当前开发工具对应的
macOS SDK，无需额外设置 `SDKROOT`；也可用 `CMAKE_OSX_SYSROOT` 显式选择系统 SDK。

## IntelliSense

C/C++ 配置由 CMake Tools 提供，不再固定读取 macOS 的 `compile_commands.json`。
首次使用或切换平台时，先运行对应 build 任务，
再执行 **CMake: Select Configure Preset** 选择对应预设，随后执行 **CMake: Configure**。
默认 SDK 目录无需配置，删除 `.build` 后仍然有效。需要其他包时，可先用
`cmake --preset <name> -DSOVKIT_SDK_ROOT=/path/to/sdk` 配置，该显式选择保留在对应构建缓存中。
不要同时启用多个 C++ 语言服务来处理同一文件。

配置字段参考 [VS Code C/C++ 调试文档](https://code.visualstudio.com/docs/cpp/launch-json-reference)
和 [CMake Tools 配置说明](https://code.visualstudio.com/docs/cpp/cmake-linux)。
