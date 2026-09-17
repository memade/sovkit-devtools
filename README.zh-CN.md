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

**SDK 获取**：本源码仓库不包含 SDK 二进制，也不自动下载 SDK。请通过 [SovKit 项目官网](https://skstu.com)了解项目，在 [仓库 issue](https://github.com/memade/sovkit-devtools/issues/new) 中向维护者询问所需平台 SDK 包的获取方式；不要将本仓库源码下载包当作可直接运行的应用。

需要 CMake ≥3.25、Ninja、Python ≥3.8、C++20 工具链、vcpkg。先取得目标平台独立 SDK 包：

```text
sdk/
  include/sovkit.h
  lib/libsovkit.dylib   # Linux: libsovkit.so
  bin/libsovkit.dll     # Windows；也兼容 lib/libsovkit.dll
  README.md            # SDK 接入文档
  LICENSE / NOTICE.md / licenses/  # 分发许可材料
```

```sh
git clone https://github.com/memade/sovkit-devtools.git
cd sovkit-devtools
export VCPKG_ROOT=/path/to/vcpkg
python3 scripts/build.py macos-arm64-debug --sdk /absolute/path/sdk --test
python3 scripts/build.py macos-arm64-release --sdk /absolute/path/sdk --test --package
```

Windows：在 x64 Developer PowerShell 中设置 `$env:VCPKG_ROOT`，使用 `python scripts/build.py windows-x64-debug --sdk C:/SDK --test`。Linux：使用 `linux-x64-debug`，先安装编译器及 wxGTK 所需开发包，例如 Debian/Ubuntu 的 `build-essential ninja-build pkg-config libgtk-3-dev libx11-dev libgl1-mesa-dev libglu1-mesa-dev`。其他预设见 CMakePresets.json；跨架构必须提供匹配 SDK。

构建目录独立为 `.build/<preset>/`，不会向源码目录生成代码。macOS 启动 `.build/<preset>/sovkit-devtools.app`；Windows/Linux 启动相应可执行文件。`--package` 生成本地 zip，不执行签名、公证、上传或发布。分发前需在目标系统验证工具链运行库及签名要求。

已有系统依赖时可直接 `cmake -S . -B .build/local -DSOVKIT_SDK_ROOT=/path/sdk -DCMAKE_PREFIX_PATH=/path/dependencies`；无 GUI 的自动化接入层可加 `-DDEVTOOLS_BUILD_GUI=OFF`。依赖路径由调用者传入，不含开发者机器绝对路径。

## 第一次手测

1. 两台电脑各自加载其平台 SDK；输入不同设备名。临时模式留空测试目录；要复现重启恢复则各选一个空目录并设置密码。
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

在 x64 Developer PowerShell 中：

```powershell
$env:VCPKG_ROOT = 'C:/path/to/vcpkg'
python scripts/build.py windows-x64-debug --sdk C:/SDK --test
& ./.build/windows-x64-debug/sovkit-devtools.exe
```

不需要 SovKit 源码或 `.lib`。SDK 目录必须包含 include/sovkit.h、README.md（接入文档），
以及 bin/libsovkit.dll 或 lib/libsovkit.dll；两处同时存在时以 lib/ 为准。
工具构建会把库复制到 exe 同目录，启动后点击“加载并检查”→ selftest → “启动身份”。
要换新 SDK，可以选择新的绝对 DLL 路径并重启工具；停止身份不等于卸载 DLL。
不要在程序运行中直接覆盖正在加载的 DLL。

未加载错误会给出 Windows 错误码：126 常见于缺依赖，193 常见于架构不匹配。
`info` 是运行时能力依据；SDK ABI 相同不表示功能版本相同。
默认是临时身份，复现断开重启问题要使用独立持久目录和密码；不要选择 Nearvia 目录。

`session_resume` 默认模板为前台恢复；按指定地址重连已有关系时，改为
`{"relationshipId":"…","address":"…","pairingPort":49152}`，不要把两种模式字段混合。
`store_result_v1` 请求接受整数或十进制字符串 ID；成功结果取出后不能重复拉同一 ID。

`ctest` 的双进程测试是本机回环，不代替 macOS ↔ Windows 真机互通或系统蓝牙验收。
蓝牙页目前只有 SDK 完整帧接口，未实现 Windows/macOS GATT 驱动。
