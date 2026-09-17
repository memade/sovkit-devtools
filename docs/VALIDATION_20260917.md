# 2026-09-17 macOS / Windows 接入验证

macOS arm64 Release 与 Windows x64 MSVC Debug 均已构建并启动 GUI；
SDK 语义版本 0.1.0、ABI 25、84 个 C 导出、58 个 JSON 操作。

## 修复

- Windows Python 3.8 / UTF-8 API 生成；识别独立 SDK 的 lib/ 与 bin/ 目录。
- CLI 使用宽字符 DLL 路径与 UTF-8 控制台；加载失败显示系统错误和 ABI 数字。
- Windows 测试目录显式归当前用户，以当前用户/SYSTEM 私有 ACL 保存；不修改公共父目录。
- 选择接收目录会切换到 transfer_decide；补齐前台恢复、会话读取与 requestId 提示。
- 更新嵌入式 SDK 接入文档，按短字节字面量嵌入中文长文档。
- 真正加载 SDK 回归还发现 SDK 的 Windows 中文接收路径问题，已在 SDK 侧修复。
  此工具使用修复后的本地 SDK；仅替换本仓库源码不能修复旧 DLL。

## 已通过

- 两端各自 core / two_peers：2/2。真实库加载、二进制往返、身份启停/重启、错误输出释放、诊断脱敏。
- 中文 DLL 路径、中文数据目录/文件名、中文消息、双端安全码确认、发送方 delivered、双方文件 completed 与内容一致。
- 持久身份/关系重开、目录互斥、错误密码不改变旧密文/数据库。
- SDK 原生单头 C/C++、错误码文档、Unicode 文件 IO/背压恢复/同名路径、bundle：两端各 4/4。
- 仅标准 C++17 + 公开头 + 动态库的随包示例：两端构建、加载、双向送达与中文文件完整性通过。
- GUI 自检两端均 code=0、symbols=84；Mac 请求/响应宽度 488/488，Windows 484/480。

通信测试为各端双进程回环，不代表两机真实网络或桌面 BLE 无线互通；
Windows 高 DPI、多机手动操作、Linux 真机与分发签名未列为本轮通过项。
工具的 MIT 不覆盖 SDK，SDK 仍按所用包许可。

本轮在开发工作区提供可执行程序，没有替换官网/近场产品已发行安装包。
