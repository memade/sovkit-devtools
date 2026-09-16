# 2026-09-16 实际验证

本轮只在 macOS arm64 完成原生构建与运行；Linux/Windows 提供源码、CMake 预设和统一构建脚本，尚未在相应系统实际编译或验证。当前主机没有运行中的 Docker daemon，也未将 Android `.so` 当成 Linux SDK。

## 已通过

- Debug / Release GUI 与 CLI 编译，两种配置的 2 项 CTest 均通过；只使用独立 SDK 包中的公开头、动态库、接入文档。开发者工具编译命令不引用 SovranKit 源码或 OrbitBridge 项目源码。
- 使用本机已有开源依赖缓存完成验证；其中 wxWidgets 缓存位于 OrbitBridge 的 vcpkg 安装目录。这只是本机传入的依赖路径，仓库源码未硬编码，成品只链接系统 frameworks / dylib。
- 全部 84 个 C 符号类型化加载；58 个 JSON 操作自动生成；ABI=25；加载后的 SDK 0.1.0 具备 `keystoreDetachVersion=1`。
- `devtools.core`：真实库加载、JSON 输出释放、二进制加解密、临时身份启停/重启、错误输出、诊断白名单。
- `devtools.two_peers`：两个独立 OS 进程、真实 LAN/Noise 配对、双方安全码确认、消息送达、20 KiB 文件逐字节一致、密码加密目录重开后身份与关系不变、目录互斥、错误密码不修改原有密文/数据库。
- GUI 实际启动、动态库加载与自检通过。检查了 macOS 窗口截图；修正初始化分栏后，请求/响应编辑器均为 488 px，窗口客户区 1260 × 800，无窄响应栏问题。
- Release ZIP 解压到独立目录后，包内 CLI / 动态库通过完整双进程通信测试，包内 GUI 启动与自检通过。包含 SDK 和工具依赖许可及 SDK 提供的第三方材料；检查未包含测试密钥、身份数据库、构建缓存或源码仓库元数据。
- 未加载 SDK 时也能安全停止/关闭；核心回归覆盖这个窗口关闭分支。
- SovKit 原仓库：单头 C/C++、动态 ABI、C++ 句柄、storage_api、business_storage 共 6 项回归通过；补强的空输出不产生身份副作用与解绑清除缓存检查也通过。

## SDK 同步修正

1. `register_keystore(NULL,NULL,NULL,NULL)` 支持安全解绑，必须已停止且业务库关闭/无 pending open；部分空回调仍拒绝。新增能力字段而不增删 C 导出。
2. C++ JSON 方法统一在入口校验并清空输出，错误时与 C 接口的所有权合同保持一致；空输出不会提前产生身份创建等副作用。
3. 接入文档补充 keystore 长度查询返回 0、实际配对 `safetyCode` 字段、文件字段、数值 requestId、日志 cursor 合同及 BLE 帧接口边界。

## 未声称完成

- Linux/Windows 实机编译、两机跨平台 GUI 手测、高 DPI、平台文件权限/签名安装验证。
- 桌面原生蓝牙 GATT 驱动、后台无线通信、WAN 网络拓扑验收。
- 正式签名、公证、提交代码、推送、公开发布。

UI 原始响应和旧日志 callback 仅在本机显示，不承诺它们是可公开分享的数据；导出按钮只产生白名单元数据。测试身份和目录在自动测试中隔离并清理，不读取 Nearvia 产品数据。
