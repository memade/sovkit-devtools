# SovKit DevTools

[English](README.md) | 简体中文 · [SovKit 官网](https://skstu.com) · [问题反馈](https://github.com/memade/sovkit-devtools/issues)

**用可见的请求、响应和事件，探索和调试 SovKit 的设备间通信。** 这是 SovKit 的开源开发者工作台，面向 SDK 接入者、通信问题排查者和愿意参与跨平台验证的贡献者。

当前版本为 **0.1.0 初版预览**，GUI 主要使用中文。macOS arm64、Windows x64 已完成原生构建、GUI 启动和独立 SDK 回环回归；两机互通/高 DPI 仍需手测，Linux 待真机验证。

独立的跨平台原生 SDK 开发者工作台。参考 OrbitBridge/brostu 的 C++、wxWidgets、libwxui 和 CMake/vcpkg 组织方式；运行和构建不需要 OrbitBridge、SovranKit 源码或 Flutter。

对 SovKit 的唯一接入材料是 **`libsovkit` 动态库、`sovkit.h` 与接入文档**。工具自身的开源 UI/JSON 依赖通过 vcpkg 获取。运行时验证 ABI 和全部 C 导出，用 `LoadLibraryExW` / `dlopen` 加载；不链接 SDK 私有静态库，Windows 不要求 SDK `.lib`。

启动身份还要求 `sovkit_info().keystoreDetachVersion >= 1`；使用带该能力的 0.1.0 SDK。旧 ABI 25 库仍可查看能力，但缺少安全解绑时不会启动测试身份。当前生成 84 个类型安全的导入、58 个 JSON 操作；回调注册、生命周期、密钥指针类接口由宿主管理，不能从任意 JSON 直接调用。

## 能做什么

- 选择 SDK 二进制、检查版本/能力/导出和加密往返；切换二进制须重启工具。
- 独立测试身份：临时内存，或密码保护的测试目录；与 Nearvia 产品数据隔离。持久目录内使用 SDK Vault、加密 keystore 和 SQLCipher。
- 按功能分组搜索 JSON API，查看/修改模板，执行并显示原始响应、状态码和耗时。
- LAN 发现、双方安全码确认、关系、消息、文件选择/接收、暂停/恢复/取消。
- 自动轮询事件；日志 v1、存储异步结果、WAN/网络分享保留手动诊断入口。
- BLE 页面是 SDK **帧接口调试**；未接入桌面系统 GATT 扫描/广播驱动，不把帧调试称为无线蓝牙验收。
- 诊断导出只包含操作名、返回码、耗时等白名单元数据；请求、消息、密钥、身份、文件路径均不导出。原始响应只在本机内存显示。
- 同一接入层提供 `sovkit-console` JSONL 入口，可写脚本和运行双进程真实通信回归。

## 构建

**SDK 来源**：默认使用 [`3rdparty/sovkit_sdk/0.1.0`](3rdparty/sovkit_sdk/0.1.0) 中由 SovKit 提供的发行文件，原样读取，不由 DevTools 构建、下载或改写。SDK 行为、接口语义、兼容性及许可均以 SovKit 项目和[随包接入文档](3rdparty/sovkit_sdk/0.1.0/SDK_INTEGRATION.md)为准；工具中的请求模板仅是调试示例。

需要 CMake ≥3.25、Ninja、Python ≥3.8、C++20 工具链、vcpkg。当前提供 Windows 和 macOS arm64 动态库；Linux 动态库交付后放入同一目录。

```text
3rdparty/sovkit_sdk/0.1.0/
  sovkit.h
  libsovkit.dll
  libsovkit.dylib       # macOS arm64
  libsovkit.so          # Linux：交付后添加，构建前需存在
  SDK_INTEGRATION.md
```

也支持 SovKit 文档中的 `include/`、`lib/`、`bin/` 布局和名为 `README.md` 的接入说明。头文件、库和说明必须来自同一份交付包。

```sh
git clone https://github.com/memade/sovkit-devtools.git
cd sovkit-devtools
export VCPKG_ROOT=/path/to/vcpkg
```

统一使用与 OrbitBridge 相同的 `cmake --preset` / `cmake --build --preset` 命令。CMake 按平台从默认目录选择 `.dll`、`.dylib` 或 `.so`，复制到 GUI 可执行文件旁，启动时自动按可执行文件所在目录加载，不依赖终端的工作目录。macOS 中两者均位于 `sovkit-devtools.app/Contents/MacOS/`。删除 `.build` 后仍使用此默认 SDK，无需重新指定路径。资源嵌入和 API 绑定生成仍需 Python。

### macOS 构建

默认使用目录中的 arm64 动态库。CMake 会在 vcpkg 探测编译器之前，自动选择当前 Xcode/Command Line Tools 对应的系统 SDK，无需手动导出 `SDKROOT`；显式设置的 `CMAKE_OSX_SYSROOT`（包括缓存选择）优先于 `SDKROOT`。

```sh
cmake --preset macos-arm64-debug
cmake --build --preset macos-arm64-debug
ctest --preset macos-arm64-debug

cmake --preset macos-arm64-release
cmake --build --preset macos-arm64-release
```

Intel Mac 使用 `macos-x64-debug` / `macos-x64-release`，首次配置时选择匹配的 x64 SDK。

```sh
cmake --build --preset macos-arm64-debug --target sovkit-devtools
cmake --build --preset macos-arm64-debug --target sovkit-console
open .build/macos-arm64-debug/sovkit-devtools.app
```

### Linux 构建

先安装编译器及 wxGTK 所需开发包，例如 Debian/Ubuntu 的 `build-essential ninja-build pkg-config libgtk-3-dev libx11-dev libgl1-mesa-dev libglu1-mesa-dev`。将 SovKit 交付的匹配架构动态库放入 `3rdparty/sovkit_sdk/0.1.0/libsovkit.so` 后，执行：

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug

cmake --preset linux-release
cmake --build --preset linux-release
```

`linux-debug` / `linux-release` 与 OrbitBridge 的名称一致，目标架构为 x64；原有 `linux-x64-debug` / `linux-x64-release` 继续可用。各预设拥有独立构建目录和 SDK 缓存。ARM64 Linux 使用 `linux-arm64-debug` / `linux-arm64-release` 并选择匹配 SDK。GUI 测试需要图形会话。

使用 `cmake --list-presets=all` 查看配置、构建和测试预设。Ninja 预设仍可使用可选的一键入口 `python3 scripts/build.py <preset> --sdk /path/sdk --test`。

Ninja 构建目录为 `.build/<preset>/`，Visual Studio 为 `out/<configuration>/`。macOS 启动 `.build/<preset>/sovkit-devtools.app`；Windows/Linux 启动相应可执行文件。macOS Release 构建完成后，可生成本地 ZIP：

```sh
cpack --config .build/macos-arm64-release/CPackConfig.cmake -B .build/packages
```

打包不执行签名、公证、上传或发布。分发前需在目标系统验证工具链运行库及签名要求。

已有系统依赖时可直接 `cmake -S . -B .build/local -DSOVKIT_SDK_ROOT=/path/sdk -DCMAKE_PREFIX_PATH=/path/dependencies`；无 GUI 的自动化接入层可加 `-DDEVTOOLS_BUILD_GUI=OFF`。依赖路径由调用者传入，不含开发者机器绝对路径。

### Windows：Visual Studio 2026 解决方案

采用与 OrbitBridge 相同的 CMake Visual Studio 生成器。安装 Visual Studio 2026 的“使用 C++ 的桌面开发”、CMake ≥4.2、Python ≥3.8 和 vcpkg；CMake、Python 需在 PATH 中。此方式不需要 Ninja 或 PowerShell。

**鼠标操作：** 在 Windows“编辑账户的环境变量”中设置 `VCPKG_ROOT`（vcpkg 根目录，SDK 默认使用仓库内的交付包），然后重新打开相关程序。双击 [`scripts/build-vs2026.bat`](scripts/build-vs2026.bat)，等待依赖安装、配置完成，脚本会打开 `out/sovkit-devtools.slnx`。选择 **Debug / x64**，执行“生成解决方案”，在 `src/app.cpp` 或 `src/sdk.cpp` 下断点并按 **F5**。若启动项不是 `sovkit-devtools`，右键该项目选择“设为启动项目”。较早版本的 CMake 可能生成 `.sln`，脚本兼容两种格式。

未设置 `VCPKG_ROOT` 时，双击脚本会尝试 `%USERPROFILE%/vcpkg`。它只生成并打开解决方案，不删除已有构建。若需手动打开，直接双击 `out/sovkit-devtools.slnx`。

CMake 默认使用 `3rdparty/sovkit_sdk/0.1.0`，并迁移旧的内置 `.sdk/windows-x64-current` 缓存路径。如需其他 SovKit 交付包，传入 `-DSOVKIT_SDK_ROOT=C:/other/sdk` 或 `scripts/build.py --sdk /path/sdk`。同名环境变量不再覆盖默认选择；显式选择会保存在构建缓存中。

也可在仓库根目录使用普通 **CMD**，按 OrbitBridge 的预设命令构建（将 vcpkg 示例路径换成实际路径）：

```bat
set "VCPKG_ROOT=C:\path\to\vcpkg"
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug

cmake --preset windows-release
cmake --build --preset windows-release
```

常用目标：

```bat
cmake --build --preset windows-debug --target sovkit-devtools
cmake --build --preset windows-debug --target sovkit-console
```

Debug/Release 共用 `out/` 解决方案，程序分别输出到 `out/Debug/` 和 `out/Release/`。GUI 构建只原样复制选定 SDK 的 DLL，以及 SovKit 随包提供的匹配 PDB（如有）。Debug/Release 只控制 DevTools；SDK 升级由 SovKit 交付新的完整包，退出工具后更换包并重新构建。

原有 `windows-x64-debug` / `windows-x64-release` 仍为 Ninja 预设，也支持配置、构建和测试的 preset 命令，或在 x64 开发者终端使用 `scripts/build.py`，输出到 `.build/<preset>/`。`windows-debug` / `windows-release` 请使用上述 CMake 命令或 Visual Studio，不经过 `build.py`。

## 发布文件与资源

Windows GUI 发布包仅含 `sovkit-devtools.exe` 和 `libsovkit.dll`。字体使用系统现有字体：
Windows 使用微软雅黑，macOS/Linux 使用各自系统默认字体；不携带字体目录。
XML、图片等 `res/` 资源、SDK 接入原文和许可说明由 `scripts/embed_assets.py` 编译进 EXE。
界面可打开“接入文档”和“关于与许可”，不依赖外部 XML、文档或资源目录。

```bat
cmake --build --preset windows-release
cpack --config out/CPackConfig.cmake -C Release -B .build/packages
```

`out/Release` 是开发构建目录，可能含 PDB、测试程序或旧资源；对外分发使用生成的 ZIP。
控制台保留为构建和测试目标，GUI 发布包不包含它。

## 第一次手测

1. 启动时自动读取可执行文件旁的平台动态库（`libsovkit.dll`、`libsovkit.dylib` 或 `libsovkit.so`）并查询版本和能力，不依赖工作目录；缺失时可手动选择。自动加载不启动身份或网络操作。两台电脑输入不同设备名。临时模式留空测试目录；要复现重启恢复则各选一个空目录并设置密码。
2. 点击“启动身份”，执行 `discovery_start`、`discovery_list`。从候选复制 `address` 与 `pairingPort` 到一端的 `pairing_start`。
3. 两端执行 `pairing_status`，当面核对 `safetyCode`（SAS），分别执行 `pairing_confirm`。确认关系出现在 `relationship_list`。
4. 把关系 ID 填进 `message_send`；接收端查看事件和 `message_list`。文件使用“选择发送文件”，接收端 `transfer_list` → `transfer_decide` 选择接收目录并接受。
5. 记录结果；遇到问题查看日志 API 和原始响应，导出诊断元数据。关闭前“停止并保存”；若停止失败保留窗口重试。

详细边界和验收见 [手测清单](docs/MANUAL_TESTING.md) 与 [本轮验证记录](docs/VALIDATION_20260917.md)。不要把多个工具进程打开到同一测试目录；进程锁会拒绝第二次打开。测试密码不支持找回，错误密码或残缺目录不会自动重置身份。

## 开源与贡献

DevTools 自有代码采用 [MIT](LICENSE) 许可；libwxui 保留 [上游 MIT 许可](3rdparty/libwxui/LICENSE) 和作者归属。SovKit SDK 单独遵循其发行包许可，工具的 MIT 许可不会覆盖 SDK。依赖说明见 [NOTICE.md](NOTICE.md)。

欢迎提交可复现的问题、平台验证结果和小范围 PR，详见 [贡献指南](CONTRIBUTING.md)。后续继续验证高 DPI、两机互通与 Linux 构建；不把未验收平台标成已支持。蓝牙系统 GATT 接入另行推进，WAN/网络分享暂不作为初版重点。

如果这个工具帮到了你，欢迎 Star、分享使用反馈，并通过 [SovKit 官网](https://skstu.com)了解底层 SDK。更新记录见 [CHANGELOG.md](CHANGELOG.md)。


## Windows SDK 手动调试

推荐使用上面的 Visual Studio 解决方案方式。以下为保留的 Ninja 构建方式，在 x64 Developer PowerShell 中：

```powershell
$env:VCPKG_ROOT = 'C:/path/to/vcpkg'
python scripts/build.py windows-x64-debug --sdk C:/SDK --test
& ./.build/windows-x64-debug/sovkit-devtools.exe
```

不需要 SovKit 源码或 `.lib`。默认包包含 sovkit.h、libsovkit.dll 和 SDK_INTEGRATION.md。
工具构建会将交付库原样复制到 exe 同目录，启动时自动加载；随后可执行 selftest 或手动“启动身份”。
要换新 SDK，可以选择新的绝对 DLL 路径并重启工具；停止身份不等于卸载 DLL。
不要在程序运行中直接覆盖正在加载的 DLL。

未加载错误会给出 Windows 错误码：126 常见于缺依赖，193 常见于架构不匹配。
`info` 是运行时能力依据；SDK ABI 相同不表示功能版本相同。
默认是临时身份，复现断开重启问题要使用独立持久目录和密码；不要选择 Nearvia 目录。

SDK 请求字段、状态码和异步行为请直接查看[随包接入文档](3rdparty/sovkit_sdk/0.1.0/SDK_INTEGRATION.md)。

`ctest` 的双进程测试是本机回环，不代替 macOS ↔ Windows 真机互通或系统蓝牙验收。
蓝牙页目前只有 SDK 完整帧接口，未实现 Windows/macOS GATT 驱动。

当前本机/Windows Debug构建、源码断点和libwxui评估见 [2026-09-20调试对齐记录](docs/DEBUG_ALIGNMENT_20260920.md)。
