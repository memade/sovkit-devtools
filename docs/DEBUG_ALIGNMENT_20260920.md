# 调试与职责边界（2026-09-20 更新）

## 当前 SDK 接入

默认使用 `3rdparty/sovkit_sdk/0.1.0` 的 SovKit 交付包，当前内容为
`sovkit.h`、`libsovkit.dll`、`SDK_INTEGRATION.md`。开发者只需这些公开交付材料；
SDK 契约与所有解释以 SovKit 项目及随包文档为准。

旧的 `.sdk/*-current` 暂存、相邻 SDK 源码重建流程已被替代，不属于 DevTools 的构建入口。
SDK 修复由 SovKit 完成并交付完整包；DevTools 原样引用，不修改头文件、动态库或说明。
本机历史 `.build/rebuild-current.ps1` 不再用于本项目的 SDK 更新流程。

Windows 构建、测试：

```bat
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
```

程序为 `out/Debug/sovkit-devtools.exe`，构建会将交付的 DLL 原样复制到同目录。
启动后自动读取 exe 目录的 `libsovkit.dll`，工作目录不影响 SDK 定位。
Debug/Release 只控制 DevTools；SDK 调试信息和对应版本均由 SovKit 提供。
默认临时测试身份；需要重启恢复时选择独立测试目录和密码。
停止身份不会卸载 DLL，替换 SDK 后需重新启动程序。

## libwxui 评估

以下表格保留早期评估。后续已按新要求完成完整 XML 界面迁移，并将窗口、文件/目录选择和消息对话框封装到 libwxui，业务层不再直接调用 wxWidgets。当前约定及测试见 [LIBWXUI.md](LIBWXUI.md)；表中“下一步”和保留业务层原生调用的建议已被替代。

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
