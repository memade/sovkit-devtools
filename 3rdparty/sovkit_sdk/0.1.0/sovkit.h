#ifndef SOVKIT_H_
#define SOVKIT_H_

/**
 * @file sovkit.h
 * @brief SovKit public C ABI and optional C++ handle contract.
 * @brief SovKit 公开 C ABI 与可选 C++ 句柄契约。
 *
 * [EN] This self-contained header is shipped with the release library. JSON
 * and text use UTF-8 with byte lengths excluding a trailing NUL. Storage,
 * vault, and key buffers are binary: preserve their exact lengths and embedded
 * zero bytes. See SDK_INTEGRATION.md for JSON schemas and numeric status codes.
 *
 * [中文] 此自包含头文件与发布版动态库一同交付。JSON 和文本使用 UTF-8，长度为
 * 不含末尾 NUL 的字节数。存储封装、Vault 和密钥缓冲区是二进制，必须保留精确
 * 长度及其中的零字节。JSON 字段与状态码数值见 SDK_INTEGRATION.md。
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#if defined(SOVKIT_BUILD)
#define SOVKIT_API __declspec(dllexport)
#else
#define SOVKIT_API __declspec(dllimport)
#endif
#define SOVKIT_CALL __cdecl
#elif defined(__clang__) || defined(__GNUC__)
#define SOVKIT_API __attribute__((visibility("default")))
#define SOVKIT_CALL
#else
#define SOVKIT_API
#define SOVKIT_CALL
#endif

/**
 * [EN] Public C ABI version. Additive C ABI changes must increment this value
 * according to the project compatibility policy.
 * [中文] 公开 C ABI 版本号。新增 C ABI 能力时，必须按项目兼容性策略递增。
 */
#define SOVKIT_ABI_VERSION 25

#ifdef __cplusplus
extern "C"
{
#endif

	/** [EN] Signed status code returned by SovKit. [中文] SovKit 返回的有符号状态码。 */
	typedef int32_t sovkit_error_t;

	/**
 * [EN] Status values are documented in SDK_INTEGRATION.md. Zero is success;
 * use sovkit_is_ok/is_error and sovkit_error_name/string/category at runtime.
 * Error definitions and lookup tables are private to the shared library.
 * [中文] 状态码数值见 SDK_INTEGRATION.md；0 表示成功。可通过下方 is_ok/is_error
 * 与 error_name/string/category 查询。错误定义与查表实现封装在动态库内部。
 */

	/**
 * [EN] Opaque handle storage. The current implementation returns an
 * ISovKit-compatible pointer; callers must not dereference it as void*.
 * [中文] 不透明句柄存储类型。当前实现返回可转换为 ISovKit* 的指针；调用方
 * 不得将其作为 void* 直接解引用。
 */
	typedef void* sovkit_handle_t;

	/** [EN] Log severity. [中文] 日志严重级别。 */
	typedef enum sovkit_log_level_t
	{
		SOVKIT_LOG_LEVEL_TRACE = 0,
		SOVKIT_LOG_LEVEL_DEBUG = 1,
		SOVKIT_LOG_LEVEL_INFO = 2,
		SOVKIT_LOG_LEVEL_WARN = 3,
		SOVKIT_LOG_LEVEL_ERROR = 4,
		SOVKIT_LOG_LEVEL_CRITICAL = 5,
		SOVKIT_LOG_LEVEL_OFF = 6,
	} sovkit_log_level_t;

	/**
 * [EN] Event wake callback. The callback receives no event payload; consume
 * events with sovkit_event_poll on the owning host thread.
 * [中文] 事件唤醒回调。回调不携带事件数据；必须在宿主所属线程通过
 * sovkit_event_poll 拉取事件。
 */
	typedef void(SOVKIT_CALL* sovkit_result_cb_t)(sovkit_error_t code,
	                                              void* user_data, const char* data,
	                                              size_t len);

	/**
 * [EN] Log callback. json_line is borrowed for the callback duration only.
 * [中文] 日志回调。json_line 仅在回调执行期间有效，调用方不得保存该指针。
 */
	typedef void(SOVKIT_CALL* sovkit_log_cb_t)(uint64_t timestamp_ms,
	                                           sovkit_log_level_t level,
	                                           const char* json_line,
	                                           size_t json_len, void* user_data);

	/**
 * [EN] With output=NULL, set required inout_size and return 0 for an existing
 * record. With a buffer, inout_size is capacity on entry and actual/required size
 * on return. A short buffer returns -10007; a missing key returns -10004.
 * Never throw across callbacks; write/remove return success only after commit.
 * [中文] output=NULL 时为长度查询：存在记录则设置所需长度并返回 0。非空缓冲区
 * 的 inout_size 输入为容量，输出为实际/所需长度；容量不足返回 -10007，记录不存在
 * 返回 -10004。回调不得抛异常；write/remove 仅在提交成功后返回 0。
 */
	typedef sovkit_error_t(SOVKIT_CALL* sovkit_keystore_read_cb_t)(
	    void* user_data, const char* key, uint8_t* output, size_t* inout_size);

	/** [EN] Host keystore write callback. [中文] 宿主密钥存储写入回调。 */
	typedef sovkit_error_t(SOVKIT_CALL* sovkit_keystore_write_cb_t)(
	    void* user_data, const char* key, const uint8_t* data, size_t size);

	/** [EN] Host keystore remove callback. [中文] 宿主密钥存储删除回调。 */
	typedef sovkit_error_t(SOVKIT_CALL* sovkit_keystore_remove_cb_t)(
	    void* user_data, const char* key);

	/**
 * @brief Register or clear the event wake callback.
 * @brief 注册或清除事件唤醒回调。
 *
 * [EN] Pass NULL to unregister. After unregistration returns, no callback is
 * executing. The callback must not re-enter registration, polling, or shutdown.
 * [中文] 传入 NULL 可注销回调。注销返回后，不再有回调执行中。回调不得重入
 * 注册、轮询或关闭接口。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL
	sovkit_register_result_cb(sovkit_result_cb_t callback, void* user_data);

	/**
 * @brief Poll one queued event.
 * @brief 拉取一条排队事件。
 *
 * [EN] Returns one SDK-allocated JSON buffer. -10004 (NOT_FOUND) means the
 * queue is empty. Release a non-NULL output with sovkit_free.
 * [中文] 返回一块由 SDK 分配的 JSON 缓冲区。-10004（NOT_FOUND）表示队列
 * 为空。非 NULL 输出必须使用 sovkit_free 释放。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_event_poll(char** out_data,
	                                                        size_t* out_len);

	/**
 * @brief Register or clear the log callback.
 * @brief 注册或清除日志回调。
 *
 * [EN] The JSON line is borrowed for the callback duration only. Pass NULL to
 * unregister and wait for the call to return before destroying user_data.
 * [中文] JSON 日志行仅在回调期间借用。传入 NULL 注销后，方可销毁 user_data。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL
	sovkit_register_log_cb(sovkit_log_cb_t callback, void* user_data);

	/** [EN] Flush logs within timeout_ms. [中文] 在 timeout_ms 内刷新日志。 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_log_flush(uint32_t timeout_ms);

	/** [EN] Stop the logging subsystem after all required flushing. [中文] 完成必要刷新后停止日志子系统。 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_log_shutdown(uint32_t timeout_ms);

	/**
 * [EN] Versioned logging v1. Configuration is JSON; the runtime ring is
 * bounded and the optional file sink is independent. Successful output is
 * SDK-owned and must be released with sovkit_free.
 * [中文] 版本化日志 v1。配置使用 JSON；运行时环形缓冲区有容量上限，可选文件
 * sink 独立运行。成功输出由 SDK 所有，必须使用 sovkit_free 释放。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_log_configure_v1(
	    const char* data, size_t len, char** out_data, size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_log_read_v1(
	    const char* data, size_t len, char** out_data, size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_log_emit_v1(
	    const char* data, size_t len, char** out_data, size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_log_stats_v1(
	    char** out_data, size_t* out_len);

	/**
 * [EN] Encrypted business storage v1. SQLite/SQLCipher is private to libdb.
 * Open before SDK startup on the keystore host thread. The key is a copied,
 * domain-separated 32-byte database key, never JSON or a password. An OK
 * submission returns a requestId, not completion; poll on the same host thread.
 * NOT_FOUND means pending. Close requires a stopped SDK and may block on I/O.
 * No legacy import, reset, or plaintext fallback is performed automatically.
 * [中文] 加密业务存储 v1。SQLite/SQLCipher 对外隐藏于 libdb。必须在 SDK 启动前、
 * 密钥存储宿主线程调用 open。key 是经域隔离的 32 字节数据库密钥，不得传入
 * JSON 或密码。返回 OK 仅表示提交成功，不表示完成；必须在同一宿主线程轮询。
 * NOT_FOUND 表示仍在处理中。close 要求 SDK 已停止，可能等待 I/O。不会自动导入
 * 旧数据、重置数据库或回退到明文。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_store_derive_key(
	    const uint8_t* data_key, uint8_t* out_database_key);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_store_open_v1(
	    const char* data, size_t len, const uint8_t* key, size_t key_len,
	    char** out_data, size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_store_query_v1(
	    const char* data, size_t len, char** out_data, size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_store_conversation_v1(
	    const char* data, size_t len, char** out_data, size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_store_result_v1(
	    const char* data, size_t len, char** out_data, size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_store_close_v1(void);

	/**
 * [EN] Register host keystore callbacks before SDK initialization. The object
 * referenced by user_data must outlive SDK shutdown and callback drain.
 * If info.keystoreDetachVersion >= 1, all four NULL arguments detach the host
 * after successful SDK shutdown AND store_close_v1. Detach clears cached
 * identity; it never deletes persisted data. Partial NULL callbacks are invalid.
 * Running, stop-pending, open/pending storage reject changes. Serialize on the
 * owning host thread; only a successful detach permits destroying user_data.
 * [中文] 在 SDK 初始化前注册宿主密钥存储回调。user_data 指向的对象必须持续存活到
 * SDK 关闭并完成回调排空之后。info.keystoreDetachVersion >= 1 时，成功 shutdown
 * 且 store_close_v1 后可传四个 NULL 解绑；清除内存身份，不删除持久数据。部分 NULL
 * 无效；运行中、停止待提交或业务库打开/待完成时拒绝变更。须在所属线程串行调用，
 * 只有解绑成功后才能销毁 user_data。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_register_keystore(
	    sovkit_keystore_read_cb_t read, sovkit_keystore_write_cb_t write,
	    sovkit_keystore_remove_cb_t remove, void* user_data);

	/**
 * [EN] Common synchronous JSON contract: input length excludes a trailing NUL;
 * NULL/0 denotes an empty object only where documented; successful output need
 * not be NUL-terminated; every non-NULL output is SDK-allocated and must be freed
 * with sovkit_free; failure clears output to NULL/0.
 * [中文] 通用同步 JSON 契约：输入长度不包含末尾 NUL；仅在明确说明时 NULL/0
 * 才表示空对象；成功输出不保证 NUL 结尾；所有非 NULL 输出均由 SDK 分配，必须
 * 使用 sovkit_free 释放；失败时输出被清零为 NULL/0。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_init(sovkit_handle_t* cpp_handle,
	                                                  const char* data, size_t len,
	                                                  char** out_data,
	                                                  size_t* out_len);

	/**
 * @brief Obtain the process-wide C++ SDK handle without starting the SDK.
 * @brief 获取进程级 C++ SDK 句柄，但不启动 SDK。
 *
 * [EN] The returned handle is valid until process termination. Use only the
 * public ISovKit interface; do not delete or free the handle.
 * [中文] 返回句柄在进程结束前有效。只能通过公开 ISovKit 接口使用；不得 delete
 * 或 free 该句柄。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL
	sovkit_init_cpp(sovkit_handle_t* out_handle);

	/**
 * @brief Stop the SDK and wait for owned workers to finish.
 * @brief 停止 SDK，并等待 SDK 自有 worker 完成退出。
 *
 * [EN] Call on the serialized keystore host thread. Check success before
 * closing the business store and detaching the keystore; a failure does not
 * permit destroying callback state or unloading the library. Unregister event
 * and log callbacks before destroying their user data.
 * [中文] 在 keystore 所属的宿主串行线程调用。成功后才能关闭业务库并解绑
 * keystore；失败不代表可以销毁回调状态或卸载库。销毁事件/日志回调的 user_data
 * 前，必须先注销相应回调。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_shutdown(void);

	/** [EN] Return whether the SDK is running. [中文] 返回 SDK 是否正在运行。 */
	SOVKIT_API bool SOVKIT_CALL sovkit_is_running(void);
	/**
 * @brief Return runtime capabilities and client defaults.
 * @brief 返回运行时能力与客户端默认配置。
 *
 * [EN] clientSettings is read-only embedded configuration. Host preferences are
 * persisted by the host; discovery_start applies candidateTtlMs on next start.
 * UI settings never disable direct-only policy, authentication, consent, or
 * integrity checks. configSha256 identifies the embedded defaults.
 * [中文] clientSettings 是只读内置配置。客户端偏好由宿主持久化；下次启动时由
 * discovery_start 应用 candidateTtlMs。UI 设置不得关闭直连策略、认证、用户同意
 * 或完整性校验。configSha256 用于标识内置默认配置。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_info(char** out_data,
	                                                  size_t* out_len);

	/**
 * @brief Probe local transport capabilities.
 * @brief 探测本机传输能力。
 *
 * [EN] The result is diagnostic only; it does not establish trust or a peer
 * relationship. Output ownership follows the common JSON contract.
 * [中文] 结果仅用于诊断，不建立信任或对端关系。输出所有权遵循通用 JSON 契约。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_transport_probe(const char* data,
	                                                             size_t len,
	                                                             char** out_data,
	                                                             size_t* out_len);

	/**
 * [EN] Identity JSON uses camelCase. Binary values use lower-case hexadecimal
 * with a Hex suffix (for example messageHex, publicKeyHex, signatureHex, and
 * blobHex). createdAt and revokedAt are unsigned Unix seconds.
 * [中文] 身份 JSON 使用 camelCase。二进制值使用小写十六进制并带 Hex 后缀，
 * 例如 messageHex、publicKeyHex、signatureHex、blobHex。createdAt 与 revokedAt
 * 使用无符号 Unix 秒数。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_identity_create(const char* data,
	                                                             size_t len,
	                                                             char** out_data,
	                                                             size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_identity_open(char** out_data,
	                                                           size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_identity_rotate_device(
	    const char* data, size_t len, char** out_data, size_t* out_len);
	/**
 * [EN] Rename the active device. deviceName is 1..64 UTF-8 bytes with no
 * control characters. Keys, IDs, and creation time remain unchanged; the new
 * signed name is observed by authenticated peers on their next connection.
 * Call on the keystore host thread.
 * [中文] 重命名当前设备。deviceName 为 1 至 64 字节 UTF-8，且不得包含控制字符。
 * 密钥、ID 与创建时间保持不变；认证对端在下次连接时获取新的签名名称。必须在
 * 密钥存储宿主线程调用。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_identity_rename_device(
	    const char* data, size_t len, char** out_data, size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_identity_revoke_active_device(
	    const char* data, size_t len, char** out_data, size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL
	sovkit_identity_list_devices(char** out_data, size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_identity_sign(const char* data,
	                                                           size_t len,
	                                                           char** out_data,
	                                                           size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_identity_verify(const char* data,
	                                                             size_t len,
	                                                             char** out_data,
	                                                             size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_identity_export(const char* data,
	                                                             size_t len,
	                                                             char** out_data,
	                                                             size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_identity_import(const char* data,
	                                                             size_t len,
	                                                             char** out_data,
	                                                             size_t* out_len);

	/**
 * [EN] Discovery returns nearby candidates only; it never establishes trust.
 * start accepts candidateTtlMs, interfaceAddress, and optional listenPort. A
 * non-zero listenPort fixes the dual-stack UDP session port.
 * [中文] 发现服务只返回附近候选，不建立信任。start 接受 candidateTtlMs、
 * interfaceAddress 以及可选 listenPort；非零 listenPort 固定双栈 UDP 会话端口。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_discovery_start(const char* data,
	                                                             size_t len,
	                                                             char** out_data,
	                                                             size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_discovery_stop(void);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_discovery_status(char** out_data,
	                                                              size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_discovery_list(char** out_data,
	                                                            size_t* out_len);

	/**
 * [EN] ABI 20 short-lived proximity invitations. create requires active LAN
 * discovery and returns a device-signed sovkit:// URI for QR, NFC NDEF, or BLE.
 * inspect authenticates and bounds the untrusted URI but never creates trust.
 * pairing_start_from_invitation pins the Noise peer to the signing device key.
 * [中文] ABI 20 短时近场邀请。create 要求 LAN 发现已启用，并返回设备签名的
 * sovkit:// URI，可通过 QR、NFC NDEF 或 BLE 传递。inspect 只认证并限制不可信 URI，
 * 不建立信任；pairing_start_from_invitation 还会将 Noise 对端绑定到签名设备密钥。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_proximity_invitation_create(
	    const char* data, size_t len, char** out_data, size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_proximity_invitation_inspect(
	    const char* data, size_t len, char** out_data, size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_pairing_start_from_invitation(
	    const char* data, size_t len, char** out_data, size_t* out_len);

	/**
 * [EN] ABI 25 host BLE GATT packet transport. Operations are open, pair,
 * receive, poll, resume, and close; route is an opaque process-local handle.
 * Hosts must obtain user consent, forward complete frames, serialize GATT
 * writes, apply backpressure, and close routes on disconnect. Limits are
 * 8 links, 65,556 bytes per packet, and 256 KiB per-link queue. BLE has no IP
 * fallback, file transfer, or network sharing. Only authenticatedRoutes may
 * resume an existing relationship. Route handles are never trust credentials.
 * [中文] ABI 25 宿主 BLE GATT 分组传输。操作包括 open、pair、receive、poll、
 * resume、close；route 是进程内不透明句柄。宿主必须先取得用户同意，转发完整帧，
 * 串行化 GATT 写入，实施背压，并在断开时关闭 route。限制为最多 8 条链路、每包
 * 65,556 字节、每链路 256 KiB 队列。BLE 不回退到 IP，不支持文件传输或网络共享。
 * 只有 authenticatedRoutes 才可恢复已有关系。route 句柄不表示信任凭据。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_ble_transport(
	    const char* data, size_t len, char** out_data, size_t* out_len);

	/**
 * [EN] F3.2 pairing. start accepts an address and pairingPort from a discovery
 * candidate. Status never exposes secret material. A relationship becomes
 * visible only after both user confirmations and both signed grants verify.
 * [中文] F3.2 配对。start 接受发现候选提供的地址和 pairingPort。status 不暴露
 * 密钥材料。仅当双方确认且双方签名授权均验证成功后，关系才可见。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_pairing_start(const char* data,
	                                                           size_t len,
	                                                           char** out_data,
	                                                           size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_pairing_status(char** out_data,
	                                                            size_t* out_len);
	/**
 * [EN] ABI 22 persisted-session projection. States are online, suspect, or
 * offline; session IDs are opaque decimal strings. Route diagnostics may expose
 * provider, path, capability, reliability, and optional cached QUIC metrics.
 * Metrics are connection-scoped, not transfer-scoped. Older SDKs may omit the
 * optional object without changing the C ABI.
 * [中文] ABI 22 持久化会话视图。状态为 online、suspect 或 offline；session ID 是
 * 不透明十进制字符串。路由诊断可返回 provider、路径、能力、可靠性以及可选的
 * QUIC 缓存指标。指标属于连接而非传输。旧版本可省略可选对象，不改变 C ABI。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_session_list(char** out_data,
	                                                          size_t* out_len);
	/**
 * [EN] ABI 21 explicit relationship resumption. Input contains relationshipId,
 * a numeric IPv4/IPv6 address, and pairingPort. Authentication uses the stored
 * relationship secret; the address is an untrusted route hint only.
 * [中文] ABI 21 显式恢复关系。输入包含 relationshipId、数字形式的 IPv4/IPv6
 * 地址和 pairingPort。认证使用已保存的关系密钥；地址仅作为不可信路由提示。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_session_resume(const char* data,
	                                                            size_t len,
	                                                            char** out_data,
	                                                            size_t* out_len);
	/**
 * [EN] ABI 23 WAN route negotiation. Offers and answers are bounded, signed
 * packages; package exchange alone cannot create a relationship or decrypt
 * data. The default product policy is direct-only; TURN and remote relay
 * candidates require an explicit experimental build. Calls may access the host
 * keystore and must run on its owning thread.
 *
 * First-contact mode uses purpose=pairing and expires signed packages within
 * 300 seconds. Async mode uses step=begin/poll and generation; cancel with
 * wan_route_stop. Optional network-path, mapping, filtering, gateway-mapping,
 * and local-port diagnostics are capability-gated by sovkit_info and are
 * diagnostic only. They do not prove reachability or authorize a peer.
 *
 * [中文] ABI 23 WAN 路由协商。offer/answer 是有界签名包；仅交换数据包不会建立
 * 关系或解密数据。默认产品策略为仅直连；TURN 和远程 relay 候选仅在显式实验构建
 * 中可用。调用可能访问宿主密钥存储，必须在其所属线程执行。
 *
 * 首次接触模式使用 purpose=pairing，签名包最长 300 秒有效。异步模式使用
 * step=begin/poll 与 generation；通过 wan_route_stop 取消。网络路径、映射、过滤、
 * 网关映射和本地端口诊断均由 sovkit_info 能力版本控制，且仅用于诊断，不能证明
 * 可达性或授权对端。详细诊断协议见 docs/NAT_CROSS_PORT_FILTERING.md 与
 * docs/IPV4_DIRECT_REACHABILITY.md。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_wan_offer_create(
	    const char* data, size_t len, char** out_data, size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_wan_offer_accept(
	    const char* data, size_t len, char** out_data, size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_wan_answer_accept(
	    const char* data, size_t len, char** out_data, size_t* out_len);
	/**
 * [EN] Return local network inventory. This call changes neither traffic nor
 * global routing. Local addresses and Android network handles are sensitive;
 * do not copy them into diagnostics. A prepared network path means only that
 * socket scoping succeeded, not that bind, reachability, or connection setup
 * succeeded.
 * [中文] 返回本地网络清单。本调用不改变流量或全局路由。本地地址及 Android 网络
 * handle 属于敏感信息，不得复制到诊断数据。网络路径状态为 prepared 仅表示 socket
 * 作用域设置成功，不表示绑定、对端可达或连接建立成功。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_network_path_list(char** out_data,
	                                                               size_t* out_len);

	/**
 * [EN] Publish changed local paths for managed-route recovery. At most four
 * paths are accepted; an empty list means offline. The update is coalesced on
 * the network loop and does not guarantee restored reachability. Credentials,
 * addresses, and platform handles are never included in recovery records.
 * [中文] 发布变化后的本地路径，用于受管路由恢复。最多接受四条路径；空列表表示
 * 离线。更新会合并到网络循环，不保证恢复可达性。凭据、地址和平台 handle 不会
 * 写入恢复记录。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_network_paths_update(const char* data,
	                                                                  size_t len);

	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_wan_route_list(char** out_data,
	                                                            size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_wan_route_stop(const char* data,
	                                                            size_t len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_pairing_confirm(const char* data,
	                                                             size_t len,
	                                                             char** out_data,
	                                                             size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_pairing_cancel(void);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_relationship_list(char** out_data,
	                                                               size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL
	sovkit_relationship_remove(const char* data, size_t len);

	/**
 * [EN] F4/F6 relationship-bound messaging. send accepts relationshipId, text,
 * and optional attachmentTransferId. It commits queued state before success
 * and does not require an online session. list may filter by relationshipId.
 * Wire signatures and envelopes are intentionally not exposed.
 * [中文] F4/F6 关系绑定消息。send 接受 relationshipId、text 以及可选的
 * attachmentTransferId。成功前提交排队状态，不要求在线会话；list 可按
 * relationshipId 过滤。消息签名和线协议封装不会暴露给调用方。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_message_send(const char* data,
	                                                          size_t len,
	                                                          char** out_data,
	                                                          size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_message_list(const char* data,
	                                                          size_t len,
	                                                          char** out_data,
	                                                          size_t* out_len);
	/**
 * [EN] Commit staged inbox/outbox changes on the calling host thread. Normally
 * event_poll performs this before returning an event.
 * [中文] 在调用方宿主线程提交暂存的收发箱变更。通常 event_poll 会在返回事件前
 * 自动执行此操作。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_message_flush(void);
	/**
 * [EN] ABI 19 deterministic, path-free export for one relationship. The output
 * contains relationship metadata, public message state, retained signed
 * envelopes, and signed transfer manifests.
 * [中文] ABI 19 为单个关系提供确定性、无路径依赖的数据导出。输出包含关系元数据、
 * 公开消息状态、保留的签名封装和签名传输清单。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_conversation_export(
	    const char* data, size_t len, char** out_data, size_t* out_len);

	/**
 * [EN] Relationship-bound file transfer. UTF-8 paths are local capabilities,
 * absent from wire records and transfer-change events. With
 * transferLocalPathsVersion=1, local response/list DTOs include localPaths;
 * treat these as sensitive and never publish raw DTOs as diagnostics. Pass only
 * application-private or user-authorized paths. Directory sources require a
 * stagingDirectory and are encoded as deterministic, hash-verified bundles.
 * asyncPreparation=true is capability-gated, applies to regular files, and
 * returns state=preparing; poll transfer_list/transfer_flush on the host thread.
 * Preparing jobs can be cancelled but not paused or resumed. decide accepts
 * transferId, accept, and destinationDirectory when accepting.
 * [中文] 关系绑定文件传输。UTF-8 路径是本机能力引用，不进入线上协议或传输变更
 * 事件；transferLocalPathsVersion=1 时，本机响应/列表 DTO 包含 localPaths。
 * 这些路径是敏感信息，不得把原始 DTO 当作可公开诊断。只能传入应用私有或用户
 * 授权路径。目录源必须提供
 * stagingDirectory，并编码为确定性、哈希校验的 bundle。asyncPreparation=true
 * 受能力版本控制，仅适用于普通文件，并返回 state=preparing；宿主线程应轮询
 * transfer_list/transfer_flush。准备中的任务可取消，但不可暂停或恢复。decide
 * 接受 transferId、accept；接受传输时还需 destinationDirectory。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_transfer_offer(const char* data,
	                                                            size_t len,
	                                                            char** out_data,
	                                                            size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_transfer_decide(const char* data,
	                                                             size_t len,
	                                                             char** out_data,
	                                                             size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_transfer_cancel(const char* data,
	                                                             size_t len);
	/**
 * [EN] ABI 18 transfer controls. pause/resume are bilateral and preserve the
 * receiver-durable chunk cursor. forget removes only terminal local history;
 * it never deletes a published destination file.
 * [中文] ABI 18 传输控制。pause/resume 为双方状态，保留接收端已持久化的分块游标。
 * forget 只删除本地终态历史，不删除已经发布的目标文件。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_transfer_pause(const char* data,
	                                                            size_t len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_transfer_resume(const char* data,
	                                                             size_t len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_transfer_forget(const char* data,
	                                                             size_t len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_transfer_list(const char* data,
	                                                           size_t len,
	                                                           char** out_data,
	                                                           size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_transfer_flush(void);

	/**
 * @brief Return the runtime C ABI version.
 * @brief 返回运行时 C ABI 版本号。
 *
 * [EN] Hosts should compare this value with SOVKIT_ABI_VERSION before using
 * version-sensitive capabilities.
 * [中文] 宿主在使用版本敏感能力前，应将此值与 SOVKIT_ABI_VERSION 比较。
 */
	SOVKIT_API uint32_t SOVKIT_CALL sovkit_abi_version(void);
	/**
 * [EN] Host storage envelope, independent of SDK startup and identity. key is
 * exactly 32 bytes supplied or unwrapped by the platform keystore. The envelope
 * uses XChaCha20-Poly1305 with a random nonce; plaintext is 1 byte..64 MiB.
 * unprotect accepts that envelope including its overhead. Outputs are binary,
 * not JSON or NUL-terminated strings.
 * Output is caller-owned and must be freed with sovkit_free. Wipe plaintext and
 * key buffers in the host. An unwrapped key is not hardware-protected in RAM.
 * [中文] 宿主存储封装，独立于 SDK 启动和身份。key 必须是平台密钥存储提供或解包的
 * 32 字节密钥。封装使用随机 nonce 的 XChaCha20-Poly1305，明文为 1 字节至 64 MiB；
 * unprotect 接受含封装开销的密文。输出为二进制，不是 JSON 或 NUL 结尾字符串。
 * 输出由调用方所有，必须用 sovkit_free 释放。宿主应清零明文和密钥缓冲区；内存中
 * 的已解包密钥不具备硬件保护。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_storage_protect(
	    const uint8_t* key, const uint8_t* data, size_t len, char** out, size_t* out_len);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_storage_unprotect(
	    const uint8_t* key, const uint8_t* data, size_t len, char** out, size_t* out_len);
	/**
 * [EN] Password envelope v1. A random 32-byte data key is protected with
 * XChaCha20-Poly1305 using a password-derived Argon2id13 wrapping key.
 * Passwords are 1..1024 UTF-8 bytes and the
 * envelope is exactly 104 bytes. Run on a worker, never a UI/network thread;
 * one KDF runs at a time. The caller owns and must wipe data_key.
 * [中文] 密码封装 v1。32 字节数据密钥由随机数生成；Argon2id13 从密码派生
 * 包装密钥，以 XChaCha20-Poly1305 保护数据密钥。密码为 1 至 1024 字节 UTF-8，
 * 封装固定 104 字节。
 * 必须在 worker 线程执行，不得运行于 UI/网络线程；同一时间只允许一个 KDF。
 * data_key 由调用方提供且必须由调用方清零。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_vault_create(
	    const uint8_t* password, size_t password_len, char** envelope,
	    size_t* envelope_len, uint8_t* data_key);
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_vault_unlock(
	    const uint8_t* password, size_t password_len, const uint8_t* envelope,
	    size_t envelope_len, uint8_t* data_key);
	/**
 * [EN] Rewrap an existing 32-byte key without mutating records. Replace the
 * persisted envelope atomically only after success.
 * [中文] 重新封装已有的 32 字节密钥，不修改业务记录。仅在成功后原子替换持久化
 * 封装文件。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_vault_rewrap(
	    const uint8_t* password, size_t password_len, const uint8_t* data_key,
	    char** envelope, size_t* envelope_len);
	/**
 * [EN] ABI messageForgetVersion=1. Removes local settled message history only.
 * Input is {relationshipId,messageId?}; omitting messageId removes all settled
 * messages in the conversation. Queued, sending, undurable records, replay
 * cursors, relationships, and received files are preserved.
 * [中文] ABI messageForgetVersion=1。仅删除本地已结算消息历史。输入为
 * {relationshipId,messageId?}；省略 messageId 时删除会话中的全部已结算消息。
 * 排队中、发送中、未持久化记录、重放游标、关系和已接收文件均保留。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_message_forget(const char* data, size_t len);
	/**
 * [EN] ABI browserNetworkVersion=1. Explicit, ephemeral browser egress for an
 * authenticated relationship. Operations are status, request, approve, deny,
 * and stop. Responses are bounded and omit URLs, contents, and credentials.
 * This is not a VPN, TURN service, system proxy, TLS interceptor, or whole-
 * device privacy boundary. Hosts must obtain informed user consent.
 * [中文] ABI browserNetworkVersion=1。为已认证关系提供显式、临时的浏览器出网能力。
 * 操作包括 status、request、approve、deny、stop。响应有界，且不返回 URL、流量内容
 * 或凭据。本能力不是 VPN、TURN、系统代理、TLS 拦截器，也不是整机隐私边界；宿主
 * 必须先取得充分告知的用户同意。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_network_share(const char* data, size_t len,
	                                                           char** out, size_t* out_len);
	/**
 * [EN] Explicitly authorized connection assistance. Hosts must report
 * foreground and permitted unmetered-network state; unknown is denied.
 * configure requires consentVersion=1. Run serialized HTTPS/control work on a
 * host worker and finish outstanding calls before SDK shutdown. Assistance does
 * not establish peer identity; signed invitations and Noise/SAS remain required.
 * [中文] 显式授权的连接协助。宿主必须报告前台状态及获准使用的非计费网络状态；
 * 未知状态按拒绝处理。configure 要求 consentVersion=1。HTTPS/控制操作必须在宿主
 * worker 上串行执行，并在 SDK 关闭前完成。连接协助不建立对端身份，仍必须使用
 * 签名邀请和 Noise/SAS。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_connection_assistance(
	    const char* data, size_t len, char** out, size_t* out_len);
	/**
 * [EN] Return the calling thread's diagnostic error slot. Not every API
 * updates this slot (notably vault/storage helpers); use the direct return
 * value of each call and the final status of asynchronous results for control.
 * [中文] 返回当前线程的诊断错误槽。并非每个 API 都更新此槽（尤其 Vault/存储
 * 辅助函数）；业务判断以该次调用的直接返回值和异步结果的最终 status 为准。
 */
	SOVKIT_API sovkit_error_t SOVKIT_CALL sovkit_get_last_error(void);

	/**
 * [EN] Free a buffer returned by SovKit. NULL is accepted.
 * [中文] 释放 SovKit 返回的缓冲区。接受 NULL。
 */
	SOVKIT_API void SOVKIT_CALL sovkit_free(void* ptr);

	/**
 * [EN] Allocate memory compatible with sovkit_free. NULL may be returned on
 * failure; the allocation is not a secure-memory primitive.
 * [中文] 分配可由 sovkit_free 释放的内存。失败时可能返回 NULL；该分配器不提供
 * 安全内存能力。
 */
	SOVKIT_API void* SOVKIT_CALL sovkit_malloc(size_t size);

	/** [EN] Borrowed static error name; never free. [中文] 借用静态错误名称，不得释放。 */
	SOVKIT_API const char* SOVKIT_CALL sovkit_error_name(int32_t code);

	/** [EN] Borrowed static error text; never free. [中文] 借用静态错误消息，不得释放。 */
	SOVKIT_API const char* SOVKIT_CALL sovkit_error_string(int32_t code);

	/** [EN] Borrowed static error category; never free. [中文] 借用静态错误类别，不得释放。 */
	SOVKIT_API const char* SOVKIT_CALL sovkit_error_category(int32_t code);

	/** [EN] Test whether code denotes an error. [中文] 判断 code 是否表示错误。 */
	SOVKIT_API bool SOVKIT_CALL sovkit_is_error(int32_t code);

	/** [EN] Test whether code denotes success. [中文] 判断 code 是否表示成功。 */
	SOVKIT_API bool SOVKIT_CALL sovkit_is_ok(int32_t code);

#ifdef __cplusplus
} // extern "C"

class ISovKit {
public:
	virtual ~ISovKit() = default;
	/**
   * [EN] C++ access to every public C capability, with identical ownership and
   * thread contracts. Existing virtual slots keep their order. Additional
   * helpers forward to the C ABI without extending the vtable; rebuild hosts
   * against this header to use them. The handle is process-wide and borrowed.
   * [中文] C++ 接口覆盖全部公开 C 能力，线程与内存所有权契约一致。保留已有虚函数
   * 顺序；补充方法直接转发 C ABI，不扩展虚表。使用新方法需重新编译宿主。
   * 句柄为进程级借用对象，不得 delete。C++ 虚接口要求兼容的编译器与 ABI。
   */
	virtual sovkit_error_t Init(const char*, size_t, char**, size_t*) = 0;
	virtual sovkit_error_t Shutdown() = 0;
	virtual bool IsRunning() const = 0;
	virtual sovkit_error_t Info(char**, size_t*) const = 0;
	virtual sovkit_error_t TransportProbe(const char*, size_t, char**,
	                                      size_t*) = 0;
	virtual sovkit_error_t IdentityCreate(const char*, size_t, char**,
	                                      size_t*) = 0;
	virtual sovkit_error_t IdentityOpen(char**, size_t*) = 0;
	virtual sovkit_error_t IdentityRotateDevice(const char*, size_t, char**,
	                                            size_t*) = 0;
	virtual sovkit_error_t IdentityRevokeActiveDevice(const char*, size_t,
	                                                  char**, size_t*) = 0;
	virtual sovkit_error_t IdentityListDevices(char**, size_t*) = 0;
	virtual sovkit_error_t IdentitySign(const char*, size_t, char**,
	                                    size_t*) = 0;
	virtual sovkit_error_t IdentityVerify(const char*, size_t, char**,
	                                      size_t*) const = 0;
	virtual sovkit_error_t IdentityExport(const char*, size_t, char**,
	                                      size_t*) = 0;
	virtual sovkit_error_t IdentityImport(const char*, size_t, char**,
	                                      size_t*) = 0;
	virtual sovkit_error_t DiscoveryStart(const char*, size_t, char**,
	                                      size_t*) = 0;
	virtual sovkit_error_t DiscoveryStop() = 0;
	virtual sovkit_error_t DiscoveryStatus(char**, size_t*) = 0;
	virtual sovkit_error_t DiscoveryList(char**, size_t*) = 0;
	virtual sovkit_error_t ProximityInvitationCreate(const char*, size_t,
	                                                 char**, size_t*) = 0;
	virtual sovkit_error_t ProximityInvitationInspect(const char*, size_t,
	                                                  char**, size_t*) = 0;
	virtual sovkit_error_t PairingStartFromInvitation(const char*, size_t,
	                                                  char**, size_t*) = 0;
	virtual sovkit_error_t PairingStart(const char*, size_t, char**,
	                                    size_t*) = 0;
	virtual sovkit_error_t PairingStatus(char**, size_t*) = 0;
	virtual sovkit_error_t SessionList(char**, size_t*) = 0;
	virtual sovkit_error_t SessionResume(const char*, size_t, char**,
	                                     size_t*) = 0;
	virtual sovkit_error_t WanOfferCreate(const char*, size_t, char**,
	                                      size_t*) = 0;
	virtual sovkit_error_t WanOfferAccept(const char*, size_t, char**,
	                                      size_t*) = 0;
	virtual sovkit_error_t WanAnswerAccept(const char*, size_t, char**,
	                                       size_t*) = 0;
	virtual sovkit_error_t WanRouteList(char**, size_t*) = 0;
	virtual sovkit_error_t WanRouteStop(const char*, size_t) = 0;
	virtual sovkit_error_t PairingConfirm(const char*, size_t, char**,
	                                      size_t*) = 0;
	virtual sovkit_error_t PairingCancel() = 0;
	virtual sovkit_error_t RelationshipList(char**, size_t*) = 0;
	virtual sovkit_error_t RelationshipRemove(const char*, size_t) = 0;
	virtual sovkit_error_t MessageSend(const char*, size_t, char**,
	                                   size_t*) = 0;
	virtual sovkit_error_t MessageList(const char*, size_t, char**,
	                                   size_t*) = 0;
	virtual sovkit_error_t ConversationExport(const char*, size_t, char**,
	                                          size_t*) = 0;
	virtual sovkit_error_t EventPoll(char**, size_t*) = 0;
	virtual sovkit_error_t MessageFlush() = 0;
	virtual sovkit_error_t TransferOffer(const char*, size_t, char**,
	                                     size_t*) = 0;
	virtual sovkit_error_t TransferDecide(const char*, size_t, char**,
	                                      size_t*) = 0;
	virtual sovkit_error_t TransferCancel(const char*, size_t) = 0;
	virtual sovkit_error_t TransferPause(const char*, size_t) = 0;
	virtual sovkit_error_t TransferResume(const char*, size_t) = 0;
	virtual sovkit_error_t TransferForget(const char*, size_t) = 0;
	virtual sovkit_error_t TransferList(const char*, size_t, char**,
	                                    size_t*) = 0;
	virtual sovkit_error_t TransferFlush() = 0;
	virtual sovkit_error_t RegisterResultCallback(sovkit_result_cb_t, void*) = 0;
	virtual sovkit_error_t RegisterLogCallback(sovkit_log_cb_t, void*) = 0;
	virtual sovkit_error_t LogFlush(uint32_t timeout_ms) = 0;
	virtual sovkit_error_t LogShutdown(uint32_t timeout_ms) = 0;
	virtual sovkit_error_t RegisterKeystore(sovkit_keystore_read_cb_t,
	                                        sovkit_keystore_write_cb_t,
	                                        sovkit_keystore_remove_cb_t,
	                                        void*) = 0;
	virtual sovkit_error_t IdentityRenameDevice(const char*, size_t, char**,
	                                            size_t*) = 0;

	// C ABI forwarding helpers; no additional virtual slots.
	static sovkit_error_t LogConfigureV1(const char* data, size_t len, char** out_data, size_t* out_len) {
		return sovkit_log_configure_v1(data, len, out_data, out_len);
	}
	static sovkit_error_t LogReadV1(const char* data, size_t len, char** out_data, size_t* out_len) {
		return sovkit_log_read_v1(data, len, out_data, out_len);
	}
	static sovkit_error_t LogEmitV1(const char* data, size_t len, char** out_data, size_t* out_len) {
		return sovkit_log_emit_v1(data, len, out_data, out_len);
	}
	static sovkit_error_t LogStatsV1(char** out_data, size_t* out_len) {
		return sovkit_log_stats_v1(out_data, out_len);
	}
	static sovkit_error_t StoreDeriveKey(const uint8_t* data_key, uint8_t* out_database_key) {
		return sovkit_store_derive_key(data_key, out_database_key);
	}
	static sovkit_error_t StoreOpenV1(const char* data, size_t len, const uint8_t* key, size_t key_len, char** out_data, size_t* out_len) {
		return sovkit_store_open_v1(data, len, key, key_len, out_data, out_len);
	}
	static sovkit_error_t StoreQueryV1(const char* data, size_t len, char** out_data, size_t* out_len) {
		return sovkit_store_query_v1(data, len, out_data, out_len);
	}
	static sovkit_error_t StoreConversationV1(const char* data, size_t len, char** out_data, size_t* out_len) {
		return sovkit_store_conversation_v1(data, len, out_data, out_len);
	}
	static sovkit_error_t StoreResultV1(const char* data, size_t len, char** out_data, size_t* out_len) {
		return sovkit_store_result_v1(data, len, out_data, out_len);
	}
	static sovkit_error_t StoreCloseV1() {
		return sovkit_store_close_v1();
	}
	static sovkit_error_t InitCpp(sovkit_handle_t* out_handle) {
		return sovkit_init_cpp(out_handle);
	}
	sovkit_error_t BleTransport(const char* data, size_t len, char** out_data, size_t* out_len) {
		return sovkit_ble_transport(data, len, out_data, out_len);
	}
	sovkit_error_t NetworkPathList(char** out_data, size_t* out_len) {
		return sovkit_network_path_list(out_data, out_len);
	}
	sovkit_error_t NetworkPathsUpdate(const char* data, size_t len) {
		return sovkit_network_paths_update(data, len);
	}
	static uint32_t AbiVersion() {
		return sovkit_abi_version();
	}
	static sovkit_error_t StorageProtect(const uint8_t* key, const uint8_t* data, size_t len, char** out, size_t* out_len) {
		return sovkit_storage_protect(key, data, len, out, out_len);
	}
	static sovkit_error_t StorageUnprotect(const uint8_t* key, const uint8_t* data, size_t len, char** out, size_t* out_len) {
		return sovkit_storage_unprotect(key, data, len, out, out_len);
	}
	static sovkit_error_t VaultCreate(const uint8_t* password, size_t password_len, char** envelope, size_t* envelope_len, uint8_t* data_key) {
		return sovkit_vault_create(password, password_len, envelope, envelope_len, data_key);
	}
	static sovkit_error_t VaultUnlock(const uint8_t* password, size_t password_len, const uint8_t* envelope, size_t envelope_len, uint8_t* data_key) {
		return sovkit_vault_unlock(password, password_len, envelope, envelope_len, data_key);
	}
	static sovkit_error_t VaultRewrap(const uint8_t* password, size_t password_len, const uint8_t* data_key, char** envelope, size_t* envelope_len) {
		return sovkit_vault_rewrap(password, password_len, data_key, envelope, envelope_len);
	}
	sovkit_error_t MessageForget(const char* data, size_t len) {
		return sovkit_message_forget(data, len);
	}
	sovkit_error_t NetworkShare(const char* data, size_t len, char** out, size_t* out_len) {
		return sovkit_network_share(data, len, out, out_len);
	}
	sovkit_error_t Assistance(const char* data, size_t len, char** out, size_t* out_len) {
		return sovkit_connection_assistance(data, len, out, out_len);
	}
	static sovkit_error_t GetLastError() {
		return sovkit_get_last_error();
	}
	static void Free(void* ptr) {
		return sovkit_free(ptr);
	}
	static void* Malloc(size_t size) {
		return sovkit_malloc(size);
	}
	static const char* ErrorName(int32_t code) {
		return sovkit_error_name(code);
	}
	static const char* ErrorString(int32_t code) {
		return sovkit_error_string(code);
	}
	static const char* ErrorCategory(int32_t code) {
		return sovkit_error_category(code);
	}
	static bool IsError(int32_t code) {
		return sovkit_is_error(code);
	}
	static bool IsOk(int32_t code) {
		return sovkit_is_ok(code);
	}
};

#endif

#endif // SOVKIT_H_
