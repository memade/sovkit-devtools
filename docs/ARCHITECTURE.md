# 构建与职责

```text
libwxui XML UI ─────────  bounded worker queue ── Sdk host
                                                    │
console JSONL ───────────────────────────────────────┤
                                                    ↓
                             typed C imports generated from sovkit.h
                                                    ↓
                             LoadLibraryExW / dlopen → libsovkit
```

`src/sdk.*` 只依赖公开头、标准库和 JSON 库；负责符号校验、精确长度输出释放、宿主 keystore、目录锁及退出顺序。SDK 的密码封装、加解密与 SQLCipher 功能都经 C ABI 调用。未复制 SDK 的协议或业务实现。

GUI 用单一工作线程串行调用 SDK，注册 keystore 与后续 store/open/result/init 均在该线程进行。日志回调只复制到有界队列，UI 不从 SDK 回调线程更新。事件每次最多取 64 条；日志队列最多 256 条，每条最多 32 KiB；UI 文本和诊断历史另有限额。

业务 UI 只调用 libwxui：完整布局在 `res/workbench.xml`，窗口、文件选择、消息对话框及线程投递由库封装，wxWidgets 调用留在 `3rdparty/libwxui`。接口及回归检查见 [libwxui 使用约定](LIBWXUI.md)。

CMake 只接受独立 SDK 包路径；生成导入和嵌入文档到构建目录。工具版本独立于 SDK，加载时检查 ABI，启动身份再检查 `keystoreDetachVersion`。更换 SDK 根目录会同步更换头、库和文档，不继承旧路径缓存；每次构建重新检查并复制运行库。

参考 brostu 后保留：原生 C++20、wxWidgets、XML 驱动 libwxui、CMake/vcpkg、多配置平台入口。简化：不用 OrbitBridge 的 gfn/xs 全局宏、libstl/libsys/libcrypt、浏览器资源或私有 SDK 源码；生成物不写进源码目录；每个平台/配置使用单独构建目录。

各平台的 vcpkg 依赖与 SDK 均须针对目标系统构建。Windows 默认静态工具依赖和 MSVC 静态运行库；SDK 分配的内存仍由 SDK 自己释放。macOS app 内置动态库；Linux 包仍要求系统 GTK/显示环境。不把另一平台同架构 `.so` 当成本机库。

发行边界：CMake/CPack 生成本地未签名候选包，不自动提交、推送或发布。macOS 签名公证和 Windows 发行签名由发行流程完成；Linux/Windows 真实测试结果须在相应系统取得。
