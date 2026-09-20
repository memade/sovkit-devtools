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

CMake 默认直接消费 `3rdparty/sovkit_sdk/0.1.0` 的 SovKit 交付包；头文件、动态库与接入文档保持原样。导入适配代码和嵌入的原文仅生成到构建目录，DevTools 不重建 SDK、不修改 SDK 头文件、不生成 SDK 规范。SDK 的接口行为、错误语义、兼容性和许可均由 SovKit 项目定义；调试模板只是使用示例。显式切换包使用 `-DSOVKIT_SDK_ROOT=...`，仍必须使用该包配套的头、库和说明。

Windows 启动时以 libwxui 返回的可执行文件路径定位同目录 `libsovkit.dll`，自动动态加载并查询版本/能力，不依赖进程工作目录，也不通过 PATH 寻库。缺失时显示预期路径并保留手动选库。加载不启动测试身份或网络操作。构建及安装只原样复制 SovKit 交付库；工具配置不会重编译或改写 SDK。运行时检查 ABI，启动身份再检查 `keystoreDetachVersion`。

API 生成器按参数类型识别 JSON 调用，忽略指针星号、逗号周围的空白和换行。格式化公开头文件不会改变方法目录。生命周期、回调、密钥和二进制缓冲接口由宿主明确管理；其他无法识别的错误码返回接口会使配置失败，避免静默生成不完整的目录和调用分发。`devtools.api_generation` 覆盖不同排版及未知签名的拒绝行为。

参考 brostu 后保留：原生 C++20、wxWidgets、XML 驱动 libwxui、CMake/vcpkg、多配置平台入口。简化：不用 OrbitBridge 的 gfn/xs 全局宏、libstl/libsys/libcrypt、浏览器资源或私有 SDK 源码；生成物不写进源码目录；每个平台/配置使用单独构建目录。

各平台的 vcpkg 依赖与 SDK 均须针对目标系统构建。Windows 默认静态工具依赖和 MSVC 静态运行库；SDK 分配的内存仍由 SDK 自己释放。macOS app 内置动态库；Linux 包仍要求系统 GTK/显示环境。不把另一平台同架构 `.so` 当成本机库。

发行边界：CMake/CPack 生成本地未签名候选包，不自动提交、推送或发布。macOS 签名公证和 Windows 发行签名由发行流程完成；Linux/Windows 真实测试结果须在相应系统取得。
