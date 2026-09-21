# 构建与职责

## 调试调用链

参考 OrbitBridge 的 `tests/sdk-demo-cpp-1`，SDK 调用使用具名函数和显式局部变量。
在下面任意一站设置断点，都可以看到选中的路径、请求文本、状态码和返回缓冲区。

```text
Frame::OnLoadSdk → Worker::ProcessJob → Sdk::load
                                         ↓
                           SdkLibrary::Open → dlopen / dlsym
                                         ↓
                             sovkit_init_cpp → ISovKit*

Frame::execute → Worker::ProcessJob → Sdk::execute → Sdk::MessageSend
                                                          ↓
                                                  handle_->MessageSend
```

界面不自动加载动态库。启动只预填 EXE 旁的完整 SDK 路径，用户可以先选择调试用的 DLL，
再点击“加载并检查”。加载会校验 ABI 和导出、取得 C++ 句柄、读取版本能力；不会初始化身份。

## 源码位置与断点

| 文件 | 职责和常用断点 |
| --- | --- |
| `src/app.cpp` | 控件绑定；`OnChooseSdk`、`OnLoadSdk`、`OnStartIdentity`、`execute`、`result` |
| `src/worker.cpp` | 串行任务；`TakeJob`、`ProcessJob`、`run`，观察局部变量 `sdk` 和 `job` |
| `src/sdk.cpp` | `Sdk::load` 取得句柄；`MakeResponse` 解析响应；析构等待停止 |
| `src/sdk/library.cpp` | `SdkLibrary::Open` 加载库、校验 ABI、解析 C 入口 |
| `src/sdk/lifecycle.cpp` | `start`、`OpenStore`、`stop`；`handle_->Init` / `Shutdown` |
| `src/sdk/operations.cpp` | 固定操作路由、自检、事件轮询 |
| `src/sdk/operations_identity.cpp` | `Info`、身份创建/打开/签名/导入导出 |
| `src/sdk/operations_network.cpp` | 发现、配对、会话、WAN、BLE、网络共享 |
| `src/sdk/operations_messages.cpp` | `MessageSend`、消息查询、文件传输 |
| `src/sdk/operations_storage.cpp` | 日志配置/查询、数据库查询/异步结果 |
| `src/sdk/profile.cpp` | 独占目录锁、口令封套、宿主密钥库回调、原子保存 |
| `src/sdk/buffer.hpp` | SDK 返回内存的释放和敏感明文擦除 |
| `src/sdk/logs.cpp` | SDK 日志回调入队，UI 稍后读取 |

例如调试发送消息：在 `Sdk::MessageSend` 的 `handle_->MessageSend(...)` 行下断点，
在 DevTools 填写请求并点击“执行请求”，然后 F11 进入 SDK 实现。
加载匹配的 DLL/PDB 并提供同次构建的源码；C++ 虚接口需要与 SDK 兼容的编译器和 C++ ABI。
DevTools 的 C ABI 版本检查不能代替编译器兼容性检查。

## 动态库和句柄

Windows 静态编译 `3rdparty/dlfcn-win32`，macOS/Linux 使用系统 `dlfcn.h`。
宿主加载代码统一为 `dlopen`、`dlsym`、`dlerror`、`dlclose`；Windows 适配层支持 UTF-8
路径并使用明确的 DLL 依赖搜索范围，不改变进程工作目录或系统代码页。
Windows 适配层沿用 MAX_PATH 的 UTF-16 文件名上限。

`ISovKit*` 来自公开的 `sovkit_init_cpp`，是 SDK 拥有的进程级借用句柄，不能 delete。
`SdkLibrary` 在取得句柄前加载失败时关闭模块；成功取得句柄后保留模块到进程退出。
正常关闭先完成 `Shutdown`、数据库关闭、密钥库解除绑定，再清除宿主数据和日志回调。

58 个工作台操作中，44 个直接通过 `ISovKit` 虚接口调用。其余 14 个在交付头文件中是
C ABI 转发辅助方法，没有虚表槽位；它们使用明确的动态函数指针。
初始化句柄、ABI 查询、错误文本、内存释放和加密存储辅助函数同样经 C 导出调用。
Windows 不需要 SDK 导入 `.lib`，也不修改头文件或复制 SDK 实现。

所有调用语句都在提交的 `.cpp` 文件中。`scripts/generate_api.py` 只生成名称、参数形态、
导出清单和数量等元数据，并核对手写路由是否完整；不再生成函数指针成员、加载代码或调用分发。
SDK 新增操作而路由未跟进时，CMake 配置直接报错，不会静默漏掉方法。
格式化头文件不会改变目录识别结果。

## 数据与线程边界

所有 SDK 业务请求在同一后台线程中执行；日志回调仅写有界队列，不访问 UI。
队列最多 16 个用户任务；事件每轮最多 64 条；日志最多 256 条，每条最多 32 KiB。
控件操作仍全部通过 libwxui，wxWidgets 留在第三方库中。

密钥库只使用空目录或 DevTools 自己的测试目录。持久模式保留独占锁、权限检查、
SDK 加密、原子写入和错误回滚；口令、密钥、明文缓冲区及时擦除。
SDK 分配的缓冲区统一由 `SdkBuffer` 调用该 SDK 的 `sovkit_free` 释放，包括异常路径。

SDK 头、库、接入文档均原样消费，接口和错误语义以 SovKit 项目为准。
构建目录统一位于 `.build`；VS 保留 `projects`、`libraries`、`3rdparty`、`tests`、
`CMakeTargets` 分组，dlfcn-win32 属于 `3rdparty`。
XML、文档及许可证编译进应用，Windows 发布包仍只有 EXE 和 SDK DLL。
