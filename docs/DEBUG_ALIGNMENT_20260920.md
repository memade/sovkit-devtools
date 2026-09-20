# 2026-09-20 调试构建与 libwxui 评估

本轮面向本机 macOS arm64、Windows x64 手工调试，不是发行包。SDK 使用最新发现层/IPv6 修改及 Windows 宏兼容修复，ABI25、84个导出、58个JSON操作。两端库按各自平台原生构建；不是把 Mac 二进制复制到 Windows。

## 构建与启动

本机已准备忽略提交的 `CMakeUserPresets.json`，复用本机现有依赖，命令：

```sh
cd /Users/martell/Developer/sovkit-devtools
python3 scripts/build.py local-macos-debug-current --sdk .sdk/macos-arm64-current --test
open .build/local-macos-debug-current/sovkit-devtools.app
```

Windows x64 Developer PowerShell：

```powershell
cd C:\Users\k34ub\Developer\sovkit-devtools
$env:VCPKG_ROOT = 'C:/Users/k34ub/Developer/vcpkg'
python scripts/build.py windows-x64-debug --sdk .sdk/windows-x64-current --test
& ./.build/windows-x64-debug/sovkit-devtools.exe
```

本机 GUI/SDK 均为 Debug。Windows GUI、SDK DLL 及匹配 `libsovkit.pdb` 也使用 Debug；GUI 自身 PDB 由 MSVC 生成。调试器可启动对应 exe/应用内部可执行文件，在 `src/app.cpp`、`src/sdk.cpp` 或 SDK 源码设置断点。VS Code 可使用已有的 C++/LLDB 调试扩展选择该程序；不要把本机私有路径提交为通用设置。

**修改 SDK 后，先退出已加载它的工具，重新编译 SDK 并刷新 `.sdk/*-current`，再构建工具。** `scripts/build.py`只负责消费独立SDK，不会暗中重建相邻SDK源码。本次在Windows保留 `.build/rebuild-current.ps1`，可在普通 PowerShell执行，自动初始化VS环境、重建当前SDK并暂存DLL/PDB，随后构建和测试工具。这个文件是本机维护辅助，不是工具的公开构建依赖。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .build/rebuild-current.ps1
```

默认临时测试身份；需要重启恢复时使用独立测试目录与密码。不要选择直予产品数据目录。停止身份不会卸载 DLL，换库必须重启工具。

## 已修复

- SDK `discovery.cpp` 的局部变量 `interface` 与 Windows SDK 宏冲突；改为 `multicast_interface`，不改变网络协议/行为。
- macOS构建入口显式使用选定Xcode的SDK，避免混用不兼容的Command Line Tools SDK。
- 构建入口允许 `--cmake-arg=-DNAME=value`；显式指定与脚本一致的构建目录，避免用户预设binaryDir导致配置/编译路径不同。
- Windows暂存SDK时同时复制存在的匹配PDB，方便源码调试。
- JSON响应区实际接入libwxui `UIManager + JsonViewer`，保留原始JSON、格式化及语法着色。GUI自检记录组件类型、原始JSON一致性和左右区域宽度。

## libwxui 评估

你的判断成立。此前业务界面只有顶部banner使用UIManager，其余布局/控件几乎全部直接写wxWidgets；不是因为libwxui缺少对应能力。

| 界面 | 已有libwxui能力 | 处理建议 |
|---|---|---|
| JSON响应 | JsonViewer：原文/格式化/紧凑、换行、着色、只读 | 本轮已接入；后续可暴露视图模式切换，并覆盖长JSON/数字精度/复制行为 |
| 请求编辑、事件和日志 | RichEdit、Edit | 下一步迁移；保留日志截断、中文输入、快捷键、只读和密码语义 |
| API导航 | TreeView、TreeNode、List | 可迁移；先补/验证方向键、Tab焦点、筛选后选中项、折叠恢复。TreeView/TreeNode当前未自行实现OnKeyDown，不能只验证鼠标点击 |
| 主布局、页签 | HorizontalLayout、VerticalLayout、TabLayout、分隔条 | 能承担页面XML；先验证高DPI、最小尺寸、拖动分隔、隐藏页原生子控件可见性 |
| 功能按钮与标签 | Button、Option、Label | 随页面XML逐步迁移，避免样式和事件逻辑散落在app.cpp |
| 文件/目录选择、系统窗口和剪贴板 | wxWidgets宿主能力 | 适合保留原生系统实现，不必为“完全不用wxWidgets”再封装一层 |

libwxui本身建立在wxWidgets上：JsonViewer/RichEdit内部也使用wxTextCtrl，UIManager继承wxPanel。目标是复用统一控件、布局和事件层，而不是禁止底层原生控件。

库层优先完善候选：键盘事件已处理返回值的传播（UIManager当前总调用evt.Skip）、TreeView键盘导航与可访问性、长JSON的格式化/着色成本和数值保真、DPI与原生子控件生命周期回归。这些是后续需逐项验证的改进点，不把它们当作已完成修复。本轮先复用已有JsonViewer，没有无依据改写第三方库内部。

## 验证边界

最终 Debug 构建验证结果：

| 验证 | macOS arm64 | Windows x64 |
|---|---|---|
| SDK discovery_service / libnet_transport | 2/2 通过 | 2/2 通过 |
| devtools core / two_peers | 2/2 通过 | 2/2 通过 |
| GUI 自检及 SDK 加载 | code=0、ABI25、84符号 | code=0、ABI25、84符号 |
| libwxui JsonViewer | 原始 JSON 一致 | 原始 JSON 一致 |
| 暂存 SDK 与程序旁 DLL/PDB | 不适用 | 两个文件哈希均匹配 |

双进程通信测试为本机回环，不等同 Mac↔Windows 真机互通或无线 BLE 测试。窗口整体观感、中文输入、复制、键盘及 DPI 等请继续手测。

Windows 保留原有仓库 HEAD，仅同步本轮需要的最新 SDK 源文件和 devtools 修改；没有覆盖其他本地工作或改写提交历史。SDK 修改后的本机重建脚本不会自动跨机器同步源码。
