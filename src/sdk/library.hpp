#pragma once
#include <sovkit.h>
#include <filesystem>
#include <string>

namespace devtools {

	// 只负责动态库和 C 入口；业务优先通过 sdk.hpp 中的 ISovKit 句柄调用。
	class SdkLibrary {
	public:
		SdkLibrary() = default;
		~SdkLibrary();
		SdkLibrary(const SdkLibrary&) = delete;
		SdkLibrary& operator=(const SdkLibrary&) = delete;
		void Open(const std::filesystem::path& path);
		void Close();
		void Retain() {
			retained_ = true;
		}

		decltype(&::sovkit_abi_version) AbiVersion = nullptr;
		decltype(&::sovkit_init_cpp) InitCpp = nullptr;
		decltype(&::sovkit_free) Free = nullptr;
		decltype(&::sovkit_error_name) ErrorName = nullptr;
		decltype(&::sovkit_error_string) ErrorString = nullptr;
		decltype(&::sovkit_storage_protect) StorageProtect = nullptr;
		decltype(&::sovkit_storage_unprotect) StorageUnprotect = nullptr;
		decltype(&::sovkit_vault_create) VaultCreate = nullptr;
		decltype(&::sovkit_vault_unlock) VaultUnlock = nullptr;
		decltype(&::sovkit_store_derive_key) StoreDeriveKey = nullptr;
		decltype(&::sovkit_store_open_v1) StoreOpenV1 = nullptr;
		decltype(&::sovkit_store_close_v1) StoreCloseV1 = nullptr;
		decltype(&::sovkit_log_configure_v1) LogConfigureV1 = nullptr;
		decltype(&::sovkit_log_read_v1) LogReadV1 = nullptr;
		decltype(&::sovkit_log_emit_v1) LogEmitV1 = nullptr;
		decltype(&::sovkit_log_stats_v1) LogStatsV1 = nullptr;
		decltype(&::sovkit_store_query_v1) StoreQueryV1 = nullptr;
		decltype(&::sovkit_store_conversation_v1) StoreConversationV1 = nullptr;
		decltype(&::sovkit_store_result_v1) StoreResultV1 = nullptr;
		decltype(&::sovkit_ble_transport) BleTransport = nullptr;
		decltype(&::sovkit_network_path_list) NetworkPathList = nullptr;
		decltype(&::sovkit_network_paths_update) NetworkPathsUpdate = nullptr;
		decltype(&::sovkit_message_forget) MessageForget = nullptr;
		decltype(&::sovkit_network_share) NetworkShare = nullptr;
		decltype(&::sovkit_connection_assistance) ConnectionAssistance = nullptr;
		decltype(&::sovkit_get_last_error) GetLastError = nullptr;

	private:
		template <class Function>
		Function Resolve(const char* name);
		void* module_ = nullptr;
		bool retained_ = false;
	};

}
