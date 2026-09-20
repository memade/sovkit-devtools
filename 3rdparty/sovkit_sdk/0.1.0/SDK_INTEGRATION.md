# SovKit SDK 接入文档

官网：https://skstu.com/ 。自有代码 Apache-2.0，第三方许可见 `licenses/` 与 `NOTICE.md`。
SDK 不依赖 Flutter/Dart；宿主通过 `include/sovkit.h` 的 C ABI 调用。

## 单头文件与 C/C++ 一致性

SDK 接入只需要目标平台的 `libsovkit` 动态库和 `sovkit.h`。头文件仅引用 C 标准头，
不再依赖或发布 `sovkit/err.h`、`plugin.h` 或其它内部头。错误常量与查表实现在动态库内部。
许可证、依赖声明、平台签名/Framework 元数据、校验清单和本文仍随发行材料保留，不是额外运行库。

当前 SDK 语义版本为 **0.1.0**，C ABI 为 **25**，公开 C 导出共 **84** 项。
SDK 语义版本、ABI、运行时能力与 Nearvia 客户端构建号是不同信息：同为 0.1.0/ABI 25 的
旧库未必包含后来加入的能力和修复。查询 `sovkit_info().sdkVersion`、
`sovkit_abi_version()` 和所需能力字段，并记录所用库的 SHA-256 / 交付来源。
本文不以客户端预览标签判断 SDK 能力。源码的语义版本来源为根目录 `VERSION`。

建议首次接入按以下顺序阅读：

1. “一份包、一个目标平台” → 构建独立示例，检查加载和生命周期。
2. “从零完成两端局域网通信” → 临时身份、双方配对、消息与文件。
3. “线程、内存与异步结果” → 把示例接入应用事件循环。
4. “加密业务库与运行日志 v1” → 持久身份、加密数据库、重启恢复。
5. “常见问题定位”与“手工调试请求” → 完善错误处理与功能调用。

C++ 句柄真实类型为 `ISovKit`（原名 `ISovSDK`）；使用旧类名的宿主需更新源码并重新编译：

```cpp
#include "sovkit.h"

sovkit_handle_t handle = nullptr;
if (ISovKit::InitCpp(&handle) == 0) {
  auto* sdk = static_cast<ISovKit*>(handle); // borrowed; never delete
  char* data = nullptr;
  size_t length = 0;
  if (sdk->IsOk(sdk->Info(&data, &length))) {
    // Consume exactly length bytes, not a NUL-terminated string.
    sdk->Free(data);
  }
}
```

现有虚函数槽位和顺序保持不变；补充的非虚转发方法，覆盖 BLE、network path、消息清理、
网络分享/协助、日志 v1、业务库、Vault、加解密、错误查询和内存管理等此前遗漏能力。
这些方法调用相同 C ABI，使用相同进程状态、线程与所有权契约。需用新头重新编译宿主才能调用。
C++ 虚表仍要求兼容工具链；跨语言或不兼容 C++ ABI 使用 C 接口。
Windows 普通常规链接需要工具链导入库（CMake `SDKDevelopment` 可选组件）；
纯运行时加载可使用 `LoadLibrary/GetProcAddress` 调用 C ABI，不要求另行分发内部头。

## 状态码合同

`sovkit_error_t` 是 `int32_t`。`0` 表示成功；`sovkit_is_ok` / `sovkit_is_error`
用于分类，`sovkit_error_name/string/category` 返回动态库内的静态字符串，不得释放。
下表符号名是诊断名称，不是 `sovkit.h` 提供的宏。需要业务分支时按本表数值判断，例如
`-10004` 表示 `event_poll` 队列为空，也表示异步 store 请求尚未完成；必须结合接口上下文解释。

<!-- BEGIN SDK ERROR CODES: resources/err.json -->
| 数值 | 诊断名称 | 说明 |
| --- | --- | --- |
| `0` | `SOVKIT_OK` | success |
| `-10000` | `SOVKIT_ERROR_INVALID_ARGUMENT` | invalid argument |
| `-10001` | `SOVKIT_ERROR_NOT_SUPPORTED` | operation not supported |
| `-10002` | `SOVKIT_ERROR_INTERNAL` | internal error |
| `-10003` | `SOVKIT_ERROR_INVALID_STATE` | invalid state |
| `-10004` | `SOVKIT_ERROR_NOT_FOUND` | not found |
| `-10005` | `SOVKIT_ERROR_ALREADY_EXISTS` | already exists |
| `-10006` | `SOVKIT_ERROR_AUTHENTICATION` | authentication failed |
| `-10007` | `SOVKIT_ERROR_BUFFER_TOO_SMALL` | buffer too small |
| `-10008` | `SOVKIT_ERROR_STORAGE_IO` | persistent storage IO failed |
| `-10009` | `SOVKIT_ERROR_STORAGE_BUSY` | persistent storage capacity or time budget exceeded |
| `-10010` | `SOVKIT_ERROR_STORAGE_CONFLICT` | persistent storage update conflict |
| `-10999` | `SOVKIT_ERROR_UNKNOWN` | unknown error |
<!-- END SDK ERROR CODES -->

SDK 错误保留范围为 `-10999..-10000`，不占用 libuv 的 `-4095..-1`。
未知码保留原数值用于诊断；`error_name` 返回 `SOVKIT_ERROR_UNKNOWN`。
异步提交返回 `0` 只说明请求被接受；仍须拉取结果并检查 JSON 中的 `status`。
从旧头迁移的源码应移除对错误宏/枚举的编译依赖，改用运行时分类或本文数值。

## 一份包、一个目标平台

包的接入文件为 `include/sovkit.h` 与 `lib/libsovkit.*`，附带 `manifest.json`、`SHA256SUMS`、许可证、libjuice/SQLCipher overlay 和原生示例。
不要将 Android 的 `.so` 用作 Linux 库，也不要把架构相同误当成 ABI 相同。
最低系统、编译配置、架构和源码摘要以该包的 manifest 为准。
解压后在包根目录运行 `shasum -a 256 -c SHA256SUMS`（Linux 可用 `sha256sum -c`）。

### 独立构建与动态加载

包根目录结构如下；示例只读取 `include/sovkit.h`，运行时显式指定动态库绝对路径。
不要求 `.lib`、manifest、私有头、Flutter、vcpkg 或 SDK 源码。

```text
sdk/
  include/sovkit.h
  lib/libsovkit.dylib       # macOS
  lib/libsovkit.so          # Linux；与 Android .so 不可互换
  bin/libsovkit.dll         # Windows；发行包也可放在 lib/ 下
  README.md                # 本文
  example/                 # 仅标准 C++17 的动态加载示例
```

macOS（先安装 Xcode Command Line Tools）/ Linux（安装 C++ 编译器）：

```sh
cd /absolute/path/sdk
cmake -S example -B example-build
cmake --build example-build
./example-build/sovkit_sdk_smoke "$PWD/lib/libsovkit.dylib"
# Linux 将末尾库名改为 libsovkit.so。
```

Windows：使用 **x64 Native Tools Command Prompt / x64 Developer PowerShell**，
安装 CMake、Ninja 和 MSVC C++ 工具集，在 PowerShell 中运行：

```powershell
Set-Location C:\SDK
cmake -S example -B example-build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build example-build
.\example-build\sovkit_sdk_smoke.exe (Resolve-Path .\bin\libsovkit.dll).Path
# 库放在 lib/ 时替换上面的 bin。使用 Visual Studio 多配置生成器时，exe 位于 Debug/。
```

smoke 成功会打印 `PASS: standalone SDK init/shutdown`；它不创建身份、发现设备或读取产品数据。
GUI 工具的构建依赖与 SDK 接入依赖不同：**SDK 示例不需要 wxWidgets**。
Android/iOS 则需在目标 App 中调用；桌面示例不代表移动平台权限或后台行为已验收。
Android 的 `System.loadLibrary("sovkit")` 对应文件 `libsovkit.so`。

Windows 动态加载的核心写法（C++ 宿主仍调用 C ABI，不涉及 C++ 虚表）：

```cpp
#include <windows.h>
#include "sovkit.h"
auto module = LoadLibraryExW(L"C:\\SDK\\bin\\libsovkit.dll", nullptr,
    LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
if (!module) { /* GetLastError(): 126 缺库/依赖，193 架构/格式不匹配 */ }
else {
  auto abi = reinterpret_cast<decltype(&sovkit_abi_version)>(
      GetProcAddress(module, "sovkit_abi_version"));
  if (!abi || abi() != SOVKIT_ABI_VERSION) { /* 拒绝继续调用 */ }
  // 对每个需要的 C 导出做相同绑定，并检查指针不为空。
  // decltype 只取得类型；直接写 sovkit_info(...) 会变成链接调用，需要导入库。
}
```

macOS/Linux 使用 `dlopen(绝对路径, RTLD_NOW | RTLD_LOCAL)` / `dlsym`；Linux 链接 `dl`。
不要同时装载两份 SDK 模块来模拟隔离实例。不要在 worker/回调仍活跃时卸载模块；
示例保持模块映射至进程结束。`char*` 路径 JSON 一律 UTF-8，Windows DLL 文件路径用 UTF-16 API。

### 从零完成两端局域网通信

两台电脑各运行 `sovkit_sdk_lan <动态库绝对路径>`（Windows 加 `.exe`）。
它会注册**进程内临时 keystore**、初始化 SDK、建立一个新的测试身份。
退出即丢失此身份与关系；再次启动需要重新配对，**不可用于生产持久身份**。
双方可以通过 `identity_rename_device` 在应用中设置不同名称；示例统一显示 SDK sample，
请按 IP/端口选择目标并当面核对安全码。

示例 stdin 每行是 `API 名称 空格 JSON`，不是 DevTools 的 JSONL 包装格式。
返回 `{"code":0,"data":...}` 中的 `code` 是 C 状态码，`data` 是该 API 原始 JSON。
日志可能混入 stdout；自动化程序只解析同时含 code、data 的行。

两端分别输入：

```text
discovery_start {"candidateTtlMs":12000}
discovery_list
```

A 从候选读取 B 的 address/pairingPort；若组播被隔离，可让 B 提供其局域网 IP 与
`discovery_start` 返回的 pairingPort。以下端口只作示例，替换为实际值：

```text
pairing_start {"address":"192.168.1.20","pairingPort":49152}
pairing_status
```

B 也输入 `pairing_status`。**两端都达到 awaiting_confirmation，安全码一致且经双方确认后**，
各自输入以下命令。不能只确认一端，也不能仅凭设备名称自动信任：

```text
pairing_confirm {"accept":true}
relationship_list
session_list
```

各端使用**自己的** relationship_list 返回的 relationshipId；不要假设两端 ID 可互换。
A 发送，B 查询；示例在每次命令前 flush 暂存状态：

```text
message_send {"relationshipId":"A端实际ID","text":"你好，SovKit🙂"}
message_list {"relationshipId":"B端实际ID"}
```

上面第一行在 A 执行，第二行在 B 执行。A 也查询自身 message_list，直到对应 messageId 的
`state` 为 `delivered`。接收方是 `received`；C 返回 0 或 state=queued 都不是已送达。
按需输入 `event_poll` 查看事件；`-10004` 只表示当前没有排队事件。正常应用必须有持续轮询，
不能像这个交互式小示例一样只等用户敲命令。

文件：A 使用已授权的绝对文件路径（Windows 可以写 `C:/Users/you/Desktop/中文.txt`）：

```text
transfer_offer {"relationshipId":"A端实际ID","sourcePaths":["/absolute/path/中文.txt"],"logicalNames":["中文.txt"],"asyncPreparation":true}
transfer_list {}
```

B 执行 transfer_list，核对文件名/体积、取得 transferId 后接受：

```text
transfer_decide {"transferId":"B端实际ID","accept":true,"destinationDirectory":"/absolute/path/inbox"}
transfer_list {}
```

两端持续查询（尤其不能只在一端轮询），最终检查状态 `completed`、接收文件大小及内容；不只看进度 100%。
接收目录必须预先存在且用户授权可写。发送准备期间尚无接收任务是正常状态；
拒绝为 `accept:false`，此时不用 destinationDirectory。最后输入 `quit` 或结束 stdin 安全退出。
随包 `python3 example/check_lan.py --exe <sovkit_sdk_lan 路径> --library <动态库路径>`
可自动验证两个进程的回环配对、双向消息送达与中文文件完整性；Windows 命令可用 python。
如需无需手敲 JSON 的调用工作台，可使用 SovKit DevTools；其 SDK 边界与本示例相同。

### 线程、内存与异步结果

把注册 keystore、开库结果消费、init、身份、配对、消息、文件、event_poll、shutdown 和解绑
串行放在**同一个宿主工作线程**。此线程可独立于 UI；不能将每次调用随机派给不同线程池 worker。
GUI 仅投递任务和接收复制后的结果，Vault KDF/停库等待不应阻塞 UI。

| 数据或回调 | 宿主处理方式 |
| --- | --- |
| JSON 输入 | UTF-8 字节数组；len 是字节数，非字符数，不含末尾 NUL；同步调用返回前保持有效 |
| JSON 输出 char** / size_t* | 先设 NULL/0；按实际长度解析，所有非空输出恰好 sovkit_free 一次；不要 strlen、free、delete |
| storage/vault 输出 | 二进制，可含零字节；保留完整长度；SDK 分配的输出同样用 sovkit_free |
| data_key / out_db_key | 宿主提供可写的 32 字节缓冲区；使用后清零，不将它写入日志或请求 JSON |
| error_name/string/category | 库内静态借用字符串；不得 sovkit_free |
| C++ handle | 进程级借用；InitCpp 不启动 SDK，不得 delete；虚接口要求兼容的 C++ ABI |
| 事件回调 | 只唤醒宿主线程，不直接操作 UI，不在回调中 poll/注销/停 SDK |
| 日志回调 | JSON 只在回调期间有效；立即复制到有界队列，返回后不能再使用原指针 |
| keystore 回调 | key 为借用字符串，value 为二进制；自行复制；不抛出异常，不重入 SDK；写/删只有提交成功才返回 0 |

不注册事件回调也可轮询：例如每 100–250 ms，在所属线程最多连续拉取 64 条 event_poll，
0 → 消费并释放；-10004 → 本轮结束；其他错误 → 记录并处理，不无限忙循环。
时间间隔和批量上限是宿主调度建议，不是 SDK 的实时性承诺。事件用于刷新，重新进入页面或
恢复连接时应重新拉取列表；不能只依赖 UI 曾收到的事件来重建业务状态。

异步 store 的 **C 返回码** 与结果 JSON 的 **status** 必须分别检查：

```text
store_query_v1(...) -> C=0, {"requestId":"17"}
store_result_v1({"requestId":17}) -> C=-10004          # 仍未完成，稍后重试
store_result_v1({"requestId":17}) -> C=0,
    {"requestId":"17","status":0,"result":{...}}    # 此时才是业务完成
```

requestId 返回为十进制字符串；请求接受非负整数或十进制字符串（避免 JS 64 位精度损失），
不得传小数、负数或空字符串。结果成功取出后即消费，重复取同一 ID 不是幂等查询，会返回
`-10000`。C=0 但内层 status<0 时是已完成的业务失败，仍须释放输出。
`get_last_error` 仅供诊断，不是所有 API 都更新；直接返回值才是该次调用的依据。

## 真正接入的顺序与责任

1. 查询 `sovkit_abi_version()` 与 `sovkit_info()`，校验所用能力和 direct-only 策略。
2. 宿主注册安全存储回调，再初始化和打开/创建身份。不得把示例内存存储用于持久身份。
3. 按公开头文件的线程合同处理通知和 `event_poll`；通知只是唤醒，不在回调中关闭 SDK。
4. 发现只提供未信任候选；配对必须核对安全码并由双方确认。用户同意后才接收文件。
5. 宿主按合同调用消息/传输 flush 提交持久状态；不要把已收到内存的数据标成可恢复进度。
6. 关闭前停止 UI 轮询并注销回调，等待 SDK 关闭后再释放宿主回调和存储。

支持 `sovkit_info().keystoreDetachVersion >= 1` 的库允许在成功 `shutdown`、
`store_close_v1` 后调用 `sovkit_register_keystore(NULL,NULL,NULL,NULL)`，明确解除 keystore
并清除内存身份。只在返回成功后销毁回调对象；运行中、停止待提交、业务库打开/正在打开时
返回 `-10003`，部分空回调返回 `-10000`。解绑不会删除用户持久文件。旧 ABI 25 二进制未必
有此行为，独立宿主应检查能力字段，不能只检查 ABI 数字。

成功返回的 JSON 是确切 UTF-8 长度，不保证以 NUL 结尾；必须用 `sovkit_free()` 释放。
当前公开入口管理进程级 SDK，不能假定多个独立租户/实例可在同一进程隔离运行。
网络权限、文件选择/导出、应用后台策略、蓝牙/NFC 平台适配和本机存储属于宿主责任。

## 加密业务库与运行日志 v1

查询 `businessStoreVersion == 1` / `runtimeLogVersion == 1`。当前 ABI 为 25；仍需逐项
核对功能字段，不只依赖 ABI 数字。底层 SQLite 操作全部归独立静态库
`3rdparty/libdb`，日志沿用 `3rdparty/liblog`，两者都嵌入 SDK，不额外加载系统 SQLite。

客户端加密模式的顺序不同于上面的最小身份宿主：

1. 获取独立的私有数据目录 D 与日志目录 L（绝对路径），使用 `sovkit_log_configure_v1` 配置日志；不可写的日志不授权更换业务目录。
2. 解锁现有密码库并注册 keystore；SDK 此时尚未启动。以 `sovkit_store_derive_key` 从 DEK
   派生独立 K_db；不能把密码当数据库密钥，也不能把密钥经 JSON/日志传递。
3. `sovkit_store_open_v1` 提交 `{version:1,path,deviceName,create:true}`，并单独传 32 字节密钥。
   返回 `{requestId:"…"}` 只代表提交；宿主立即清理自己的临时密钥副本。
4. 在原提交宿主线程轮询 `sovkit_store_result_v1({requestId})`：`NOT_FOUND` 表示尚未完成；
   `OK` 返回 `{requestId,status,result}`，仍须检查里面的 `status`。打开成功后才启动 SDK。
5. 历史经 `sovkit_store_query_v1` 白名单类型分页，会话经 `sovkit_store_conversation_v1`
   读取/保存；同样按 requestId 等待实际结果。业务消息/传输只能经各自业务 API，不提供任意 SQL。
6. 退出先停止新业务请求、提交草稿并等待已接受命令，停止 SDK，再在非 UI 线程等待
   `sovkit_store_close_v1`。只有这些步骤成功后才释放密码库、DEK 和进程锁。

### 持久模式的首次建立与再次打开

- 首次建立：先由用户选择应用私有目录与密码，调用 vault_create 得到**随机 DEK**与 104 字节
  二进制 envelope；将 envelope 原子保存。回调的 keystore 内容由宿主按 key → 二进制 value
  保存，使用 storage_protect(DEK, ...) 加密后持久提交。示例内存 map 不能直接用于此模式。
- 再次打开：读取同一个 envelope，通过 vault_unlock 恢复**原 DEK**，再解密原 keystore。
  密码错误返回 -10006，应保留所有原文件；文件损坏、目录非空或身份打开失败不能自动创建新身份。
- 数据库：register_keystore 后用 store_derive_key 派生 32 字节 K_db，然后 store_open_v1；
  输入数据路径为 `D/sovkit.db`。第一次选择 create=true；已有数据应保留原库和原 keystore
  引导记录，不可用“删除数据库再打开”处理错误。不要向调用栈不同的线程转移待完成的 open。
- 改密码：vault_rewrap 用新密码重新包装**同一个 DEK**；成功后原子替换 envelope，不重新生成
  DEK、不重建库。写盘失败时保留旧 envelope；宿主负责文件备份、进程互斥和失败恢复。
- 退出：成功 shutdown → store_close_v1 → register_keystore 四个 NULL 解绑后，清零宿主 DEK、
  K_db 和已解密的 keystore 缓冲区，再释放回调对象。日志/事件回调另行注销并等待完成。
  不要用 get_last_error 代替这些函数的直接返回值。

数据库加密不包含接收文件本体或普通日志文件；它们的访问权限、备份与清理由宿主管理。
路径必须是用户授权的本机私有目录；“可写”并不足够：

- macOS/Linux：数据库父目录归当前用户且权限为 0700，数据库及侧车为 0600；不能用软链接替代。
- Windows：数据库父目录所有者为当前用户的 TokenUser SID，DACL 只允许该用户与 SYSTEM，
  不得继续继承 Users/Administrators 的访问 ACE。为新目录用 CreateDirectoryW 的
  SECURITY_ATTRIBUTES 提供 SDDL `O:<SID>D:P(A;OICI;FA;;;<SID>)(A;OICI;FA;;;SY)`；
  `<SID>` 替换为当前 TokenUser SID。已有目录先检查所有者，只收紧用户明确授权的应用私有
  目录；不要接管别人的目录、修改父级公共目录或关闭系统安全检查。

SDK 为其创建的数据库文件设置权限，但会拒绝不符合上述要求的已有父目录；宿主应先准备好
目录再开库。DevTools 已实现这个准备步骤。不要通过整体放宽 ACL 来掩盖 -10008。

成功停止会释放服务持有的业务模型与数据库引用，同进程重开会重新加载。
停止后若再次调用离线业务列表等接口重新加载服务缓存，应再次停止 SDK 后再关库；
`is_running=false` 本身不代表已完成持久化及状态释放。

新结果接口使用 owned、非 NUL 终止 JSON；用确切长度读取并 `sovkit_free`。未完成请求槽
有上限，宿主停拉会明确背压。已提交的写入不能因 UI 超时就假定已回滚。关闭错误不能当作
安全卸载许可。keystore 回调仍仅在其所属宿主线程运行；不要直接在任意 isolate 执行身份操作。

运行日志使用 `sovkit_log_read_v1` 按 session/afterSeq 拉取，各消费者有独立游标；
`sovkit_log_stats_v1` 提供文件可用性、队列及丢弃信息。文件日志默认是脱敏 JSONL，不是
密码库加密内容。现有借用文本日志回调不能直接接到延迟执行的 Dart listener。

纯身份/探测原生宿主仍可使用既有模式；进程一旦尝试客户端数据库引导，就不再回退旧业务
keystore 快照。旧研发业务数据不会自动导入、覆盖或删除。详情及实际验收范围见仓库的
`docs/SQLITE_LOGGING_DESIGN.md` 与 `docs/SQLITE_LOGGING_VALIDATION_20260910.md`。

## 默认配置与用户覆盖

`sovkit_info().clientSettings` v1 返回内置默认值、候选 TTL 范围，`configSha256` 标识配置。
用户偏好由宿主保存；`candidateTtlMs` 在下一次 `sovkit_discovery_start` 时传入。
主题、自动开始发现、接收目录策略和高级入口仅属于客户端；不是全局改写 SDK 的 API。
不提供关闭认证、最终校验或接收确认的用户覆盖。近场正式客户端不开放跨网与协助入口；
SDK 中保留的连接协助只能通过独立的显式授权及短期预约使用，不是全局放开中继。

## 从源码生成包

先用 `scripts/build_sdk.sh` 构建对应 Release SDK（或现有 `scripts/build.sh` 构建客户端），再运行：

```sh
python3 scripts/package_sdk.py macos
python3 scripts/package_sdk.py android-arm64
```

脚本核验当前源码摘要、二进制哈希、Release、精确导出与纯直连；仅打包指定切片，输出到新的
`.build/sdk-packages/` 目录及 zip，不覆盖旧包。默认明确标为 development-preview。
`--official` 另要求已提交的干净工作树及正式发布追溯门禁；不代替签名、公证、安全或许可审核。
libjuice 受覆盖源码的获取方式见 NOTICE；overlay 原样随包附带，不冒充完整上游源码镜像。

第一版独立 Dart/Flutter pub 包仍未交付；现有 Flutter 适配位于客户端内。原生 SDK 包可单独使用。

## 暂缓的网络能力（SDK 保留，近场产品不开放）

Nearvia 免费首版范围为局域网消息/文件与 BLE 短消息。网络分享、WAN 配对与连接协助
不出现在正式入口，也不会自动恢复旧协助设置。研发入口和 SDK API 保留以便未来继续验证。
BLE 不支持文件、多跳 Mesh 或转为 IP 的隐式回退。

### 浏览器借网实验

`sovkit_info().browserNetworkVersion == 1` 表示具备新增 `sovkit_network_share` 控制入口。
经出口方单独批准后，SDK 提供限时、限量、公网 TCP 转发。配对本身不授予出口权限；
只接受已认证 QUIC 直连，断线停止，不回退本机出口。`status/request/approve/deny/stop`
的 JSON 合同见公共头文件；返回 JSON 仍需检查 `ok` 和 `state`，并调用 `sovkit_free`。
两端须使用本轮 SDK。本机 SOCKS5 无进程级认证，宿主须明确提示并隔离浏览器配置，
不能把它宣称为 VPN 或整个操作系统的隐私隔离。原生 SDK 无 Flutter 依赖。
仓库内的实现边界与手测清单见 `docs/tasks/BROWSER_NETWORK_SHARING.md`。

## C ABI 与 C++ 方法对应表

独立原生开发者工具接入与可执行 JSON 示例见下文“手工调试请求”。SDK 消费者应以此公开文档和 `sovkit.h` 为合同，不依赖 Flutter 适配或 SDK 内部头文件。

`Init` 执行 SDK 初始化；`InitCpp` 只取得借用句柄，不启动 SDK。其余输入/输出与同名 C 能力一致。

| C ABI | ISovKit 方法 |
| --- | --- |
| `sovkit_register_result_cb` | `RegisterResultCallback` |
| `sovkit_event_poll` | `EventPoll` |
| `sovkit_register_log_cb` | `RegisterLogCallback` |
| `sovkit_log_flush` | `LogFlush` |
| `sovkit_log_shutdown` | `LogShutdown` |
| `sovkit_log_configure_v1` | `LogConfigureV1` |
| `sovkit_log_read_v1` | `LogReadV1` |
| `sovkit_log_emit_v1` | `LogEmitV1` |
| `sovkit_log_stats_v1` | `LogStatsV1` |
| `sovkit_store_derive_key` | `StoreDeriveKey` |
| `sovkit_store_open_v1` | `StoreOpenV1` |
| `sovkit_store_query_v1` | `StoreQueryV1` |
| `sovkit_store_conversation_v1` | `StoreConversationV1` |
| `sovkit_store_result_v1` | `StoreResultV1` |
| `sovkit_store_close_v1` | `StoreCloseV1` |
| `sovkit_register_keystore` | `RegisterKeystore` |
| `sovkit_init` | `Init` |
| `sovkit_init_cpp` | `InitCpp` |
| `sovkit_shutdown` | `Shutdown` |
| `sovkit_is_running` | `IsRunning` |
| `sovkit_info` | `Info` |
| `sovkit_transport_probe` | `TransportProbe` |
| `sovkit_identity_create` | `IdentityCreate` |
| `sovkit_identity_open` | `IdentityOpen` |
| `sovkit_identity_rotate_device` | `IdentityRotateDevice` |
| `sovkit_identity_rename_device` | `IdentityRenameDevice` |
| `sovkit_identity_revoke_active_device` | `IdentityRevokeActiveDevice` |
| `sovkit_identity_list_devices` | `IdentityListDevices` |
| `sovkit_identity_sign` | `IdentitySign` |
| `sovkit_identity_verify` | `IdentityVerify` |
| `sovkit_identity_export` | `IdentityExport` |
| `sovkit_identity_import` | `IdentityImport` |
| `sovkit_discovery_start` | `DiscoveryStart` |
| `sovkit_discovery_stop` | `DiscoveryStop` |
| `sovkit_discovery_status` | `DiscoveryStatus` |
| `sovkit_discovery_list` | `DiscoveryList` |
| `sovkit_proximity_invitation_create` | `ProximityInvitationCreate` |
| `sovkit_proximity_invitation_inspect` | `ProximityInvitationInspect` |
| `sovkit_pairing_start_from_invitation` | `PairingStartFromInvitation` |
| `sovkit_ble_transport` | `BleTransport` |
| `sovkit_pairing_start` | `PairingStart` |
| `sovkit_pairing_status` | `PairingStatus` |
| `sovkit_session_list` | `SessionList` |
| `sovkit_session_resume` | `SessionResume` |
| `sovkit_wan_offer_create` | `WanOfferCreate` |
| `sovkit_wan_offer_accept` | `WanOfferAccept` |
| `sovkit_wan_answer_accept` | `WanAnswerAccept` |
| `sovkit_network_path_list` | `NetworkPathList` |
| `sovkit_network_paths_update` | `NetworkPathsUpdate` |
| `sovkit_wan_route_list` | `WanRouteList` |
| `sovkit_wan_route_stop` | `WanRouteStop` |
| `sovkit_pairing_confirm` | `PairingConfirm` |
| `sovkit_pairing_cancel` | `PairingCancel` |
| `sovkit_relationship_list` | `RelationshipList` |
| `sovkit_relationship_remove` | `RelationshipRemove` |
| `sovkit_message_send` | `MessageSend` |
| `sovkit_message_list` | `MessageList` |
| `sovkit_message_flush` | `MessageFlush` |
| `sovkit_conversation_export` | `ConversationExport` |
| `sovkit_transfer_offer` | `TransferOffer` |
| `sovkit_transfer_decide` | `TransferDecide` |
| `sovkit_transfer_cancel` | `TransferCancel` |
| `sovkit_transfer_pause` | `TransferPause` |
| `sovkit_transfer_resume` | `TransferResume` |
| `sovkit_transfer_forget` | `TransferForget` |
| `sovkit_transfer_list` | `TransferList` |
| `sovkit_transfer_flush` | `TransferFlush` |
| `sovkit_abi_version` | `AbiVersion` |
| `sovkit_storage_protect` | `StorageProtect` |
| `sovkit_storage_unprotect` | `StorageUnprotect` |
| `sovkit_vault_create` | `VaultCreate` |
| `sovkit_vault_unlock` | `VaultUnlock` |
| `sovkit_vault_rewrap` | `VaultRewrap` |
| `sovkit_message_forget` | `MessageForget` |
| `sovkit_network_share` | `NetworkShare` |
| `sovkit_connection_assistance` | `Assistance` |
| `sovkit_get_last_error` | `GetLastError` |
| `sovkit_free` | `Free` |
| `sovkit_malloc` | `Malloc` |
| `sovkit_error_name` | `ErrorName` |
| `sovkit_error_string` | `ErrorString` |
| `sovkit_error_category` | `ErrorCategory` |
| `sovkit_is_error` | `IsError` |
| `sovkit_is_ok` | `IsOk` |

## 手工调试请求（独立原生宿主）

所有示例均为 UTF-8 JSON 对象，传入长度不包含末尾 NUL。下面省略 `sovkit_` 前缀；
`*_list` 等返回的 JSON 应按实际长度读取。宿主须自行管理 keystore、线程及用户授权。
开发者工具应使用独立测试身份和目录，不能直接打开 Nearvia 的正式数据。

keystore 的 `read(user,key,NULL,&size)` 是长度查询：存在记录时设置长度并返回 **0**，
不能返回 BUFFER_TOO_SMALL；实际读入的缓冲区不足时才返回 `-10007`。不存在返回 `-10004`。
宿主回调不得跨 C ABI 抛出异常；写入/删除只有持久提交成功才返回 0。

### 生命周期、发现与确认

先注册 keystore；持久业务库按前面的顺序打开，再 `init({})`，然后 `identity_open()`。
仅在返回 `-10004` 时才可由宿主选择创建新测试身份：`identity_create({"deviceName":"DevTools-A"})`。
其他错误不得当成空身份自动覆盖。`init_cpp` 仅取得句柄，不代替 `init`。

```json
{"candidateTtlMs":12000}
```

将上面的对象交给 `discovery_start`；输出含 `pairingPort`。`discovery_list()` 返回候选列表，
候选含 `address` / `pairingPort`。一端据候选调用 `pairing_start`：

```json
{"address":"192.168.1.20","pairingPort":49152}
```

双方轮询 `pairing_status()`；状态到 `awaiting_confirmation` 后，双方当面核对 **`safetyCode`**，
再分别调用 `pairing_confirm({"accept":true})`。`accept:false` 表示拒绝；`pairing_cancel()` 取消。
不能仅凭发现候选、相同地址或收到邀请就自动确认。成功关系通过 `relationship_list()` 查询，
使用其中的 `relationshipId`；连接状态通过 `session_list()` 查询。

候选 JSON 的 `knownRelationshipId` 和 `observedAddresses` 应保留到界面模型：前者可与已有
联系人匹配，后者用于同一设备多接口地址的显示去重。不要在解码时丢弃它们，让恢复中的
旧联系人变成“新设备”。这些字段用于展示关联，不等于 `trusted`，也不能替代会话身份验证。

### 从后台或锁屏返回

宿主在应用实际从后台/挂起返回前台、业务存储仍已打开时，调用
`session_resume({"op":"foreground"})`；能力可通过 `info.foregroundRecoveryVersion == 1`
检查。权限弹窗引起的短暂失焦不应反复触发恢复。返回 `{"accepted":true}` 只表示恢复请求
已排队，须继续读取发现列表、会话和消息状态确认结果，不能直接显示已连接或已送达。

当前源码的 iOS 恢复逻辑会在 SDK 工作线程重建发现和局域网通信的 UDP 套接字、恢复组播，保留原端口、发现标识、
已有关系和待确认状态；无需重建身份、清库或自动确认安全码。宿主明确停止发现后，前台恢复
不会自行重新开启发现。此行为不承诺锁屏/后台实时可达，且不能由 ABI 25 或版本号 0.1.0
推定旧二进制已含修复，应核对交付包的构建来源和 SDK 摘要。

### 消息与文件

`message_send`：

```json
{"relationshipId":"从 relationship_list 复制","text":"你好，SovKit"}
```

`message_list({"relationshipId":"…"})` 返回消息列表。发送成功表示已提交排队，
送达需检查事件/消息状态。`event_poll()` 在返回事件前会提交暂存收发状态；不消费事件的宿主
也应周期性在所属线程调用 `message_flush()` / `transfer_flush()`。

`transfer_offer` 的 `sourcePaths` 与 `logicalNames` 数量须对应：

```json
{"relationshipId":"…","sourcePaths":["/absolute/path/test.bin"],"logicalNames":["test.bin"],"asyncPreparation":true}
```

异步准备返回不等于已发送。使用 `transfer_list({})` 查询状态；接收方核对请求后调用
`transfer_decide`，接受时必须给出已授权的目录：

```json
{"transferId":"从 transfer_list 复制","accept":true,"destinationDirectory":"/absolute/path/inbox"}
```

拒绝使用 `{"transferId":"…","accept":false}`。`transfer_cancel/pause/resume/forget`
均接受 `{"transferId":"…"}`；forget 仅适用于终态历史，不删除已发布接收文件。
Windows 路径在 JSON 中须正确转义，或使用可接受的正斜杠绝对路径。BLE 不支持文件传输。
`info.transferLocalPathsVersion == 1` 时，响应/列表的 `localPaths` 包含**本机**路径，
发送方为源路径，接收方为目标路径；同名文件可能改名，不能自己拼接出最终路径。
只有 incoming/completed 后才把接收文件作为可打开的成品。此字段不传给对端，也不在
transfer.changed 事件中，但原始 API JSON 不能直接当作可公开诊断。

传输中进程结束后，重新打开原业务存储并恢复已认证关系，使用原 `transferId` 查询恢复结果。
恢复以接收端已持久化的分块边界为准，未完成的分块可能重传；不能将内存中已收字节当作已落盘进度。
当前源码在重连时丢弃接收端旧会话的未完成分块缓存，保留已提交进度，避免与重启发送端的新窗口冲突。
最终完成仍须核对双方任务状态和文件完整性；交付包是否包含此修复以构建来源为准。

### 业务库异步查询与日志

`store_query_v1` 的字段是 **`kind`**，支持 `message` / `transfer`，不是通用 SQL 或关系列表：

```json
{"kind":"message","relationshipId":"…","limit":50}
```

返回 requestId 后，通过 `store_result_v1({"requestId":1})` 拉取（1 替换为实际 ID；也可使用十进制字符串）；`-10004` 表示未完成。
最终返回的 `data` 本体含 `requestId/status/result`，必须再检查业务 `status`。
`store_query_v1` 可用 `beforeId` 向前翻页，传上页 `result.nextCursor`；没有下一页时停止。
limit 为 1..200；不要对 message_list 假设同样支持分页字段。

`store_conversation_v1` 读取：`{"relationshipId":"…","action":"read"}`。
完成结果的 `draftRevision` / `lastReadLocalId` 是十进制字符串。
保存草稿：`{"relationshipId":"…","action":"save","draft":"草稿","expectedRevision":"0"}`，
其中 expectedRevision 必须取最近一次读取的 draftRevision；冲突 status=-10010 时先重读，
不要不加判断地覆盖用户的新草稿。`lastReadLocalId` 可单独随 action=save 更新，不提供 draft
时无需 expectedRevision。这些字段支持非负整数或十进制字符串。

`log_configure_v1` 必须提供绝对日志目录；日志是脱敏明文，宿主应使用独立目录：

```json
{"version":1,"directory":"/absolute/path/devtools-logs","role":"helper","console":false,"minimumLevel":"info"}
```

`role` 支持 `app/helper/sdk/sovkitd/stun`。`log_read_v1` **不传 version**，游标是十进制字符串：

```json
{"afterSeq":"0","maxCount":100}
```

后续读取须将响应中的 `session` 与 `nextCursor` 作为下次的 `session` / `afterSeq`；支持 `minimumLevel/maxBytes`，不存在 `limit` 字段。
旧日志 callback 的 JSON 只在回调期间有效，宿主应立即复制到有界队列；不能在回调中调用 UI。

### BLE 手动帧入口的边界

`ble_transport({"op":"open"})` 返回 route；`{"op":"pair","route":"…"}` 发起配对，
`{"op":"poll"}` 取出待发完整帧，宿主将其送给另一端的
`{"op":"receive","route":"另一端 route","packetHex":"…"}`；断开时执行 close。
route 为进程内句柄，不能跨进程复用。该接口本身不扫描蓝牙、不建立 GATT 链接，
桌面工具只实现帧调用时不能宣称已有真实无线蓝牙支持。


## 常见问题定位

| 现象 | 检查顺序 |
| --- | --- |
| Windows 无法加载 / 126 | DLL 本体是否在指定绝对路径；用 dumpbin /DEPENDENTS 检查依赖；不要从随机网站补 DLL |
| Windows / 193 或 macOS wrong architecture | App 与 SDK 的 x64/arm64、目标 OS 是否一致；Android arm64 不是 macOS arm64 |
| 编译报 unresolved external sovkit_xxx | 动态加载模式要调用 GetProcAddress 取得的函数指针；直接调用 C++ 转发方法也可能引入链接符号 |
| 启动成功却不能创建身份 | 先注册全部三种 keystore 回调；检查长度查询必须返回 0，以及线程是否变化 |
| -10003 INVALID_STATE | 生命周期、所属线程、身份或业务库是否未就绪；不要用反复 init/新建身份掩盖状态错误 |
| -10008 / -10009 | 分别检查目录/磁盘写入与占用、未消费的异步请求/背压；不要自动清库重试 |
| 局域网列表空 | 双方都启用发现；同一 LAN，无访客/AP 隔离；允许本地网络与应用防火墙；检查 VPN/虚拟网卡 |
| 能发现却配对失败 | 使用 pairingPort，不把发现端口当会话端口；双方安全码与确认；旧关系先查看 session_list |
| 发送返回 0，却未送达 | 查 messageId 的 state 与 session_list；在宿主线程持续 poll/flush；离线排队不承诺后台必达 |
| 文件准备/接收一直不动 | poll/transfer_flush 是否持续；接收端是否明确接受；源文件稳定且接收目录存在、有空间/权限 |
| Windows 中文路径失败 | UTF-8 JSON，反斜杠正确转义；LoadLibraryExW 接收 UTF-16；不要把当前 ANSI codepage 当 UTF-8 |
| 关闭失败 | 保持 callback/user_data/密钥与模块有效，恢复可写性后重试；不能先 free 再等 worker |

macOS 宿主若启用 App Sandbox，需自行配置网络客户端/服务器及文件选择授权；动态库不会代替
App 取得系统权限。Windows 防火墙只为当前测试程序在所需网络范围授权，不需要整体关闭防火墙。
BLE 需宿主实现系统 GATT 桥；DevTools 的帧操作不代表该桌面工具已实现蓝牙无线通信。

接入验收至少包括：两端安全码、双向 Unicode 消息与 delivered、用户确认收文件及内容校验、
断开/恢复、错误密码不改原数据、重启仍为原身份、停止失败保留回调状态。
诊断记录 SDK 版本/ABI/库摘要、系统架构、API 名、状态码、顺序与耗时；不要公开密码、密钥、
消息正文、身份备份或私人路径。未测试的跨平台组合不要因单机回环通过就标为互通已验收。
