#include "library.hpp"
#include "../sdk.hpp"
#include <dlfcn.h>
#include <stdexcept>

namespace devtools {

	template <class Function>
	Function SdkLibrary::Resolve(const char* name) {
		// 清除旧错误，确保本次 dlsym 的错误不会被上次调用干扰。
		dlerror();
		void* address = dlsym(module_, name);
		const char* error = dlerror();
		if (error || !address) {
			throw std::runtime_error(std::string("Missing public symbol: ") + name + (error ? std::string("; ") + error : ""));
		}
		return reinterpret_cast<Function>(address);
	}

	void SdkLibrary::Open(const std::filesystem::path& path) {
		if (module_) {
			throw std::runtime_error("Restart the tool to switch SDK binaries");
		}
		// 明确加载用户选中的绝对路径，不使用进程当前目录搜索 SDK。
		if (!path.is_absolute() || !std::filesystem::is_regular_file(path)) {
			throw std::runtime_error("Select an absolute SDK library file");
		}
		retained_ = false;
		const std::string filename = utf8(path);
		module_ = dlopen(filename.c_str(), RTLD_NOW | RTLD_LOCAL);
		if (!module_) {
			const char* error = dlerror();
			throw std::runtime_error(std::string("Cannot load SDK: ") + (error ? error : "unknown loader error"));
		}
		try {
			// 先检查 C ABI，版本不匹配时绝不取得或调用 C++ 虚接口。
			AbiVersion = Resolve<decltype(AbiVersion)>("sovkit_abi_version");
			const uint32_t runtime_abi = AbiVersion();
			if (runtime_abi != SOVKIT_ABI_VERSION) {
				throw std::runtime_error("SDK/header ABI mismatch: runtime=" + std::to_string(runtime_abi) + ", header=" + std::to_string(SOVKIT_ABI_VERSION));
			}
			// 这里只生成导出名称数据；真正的 SDK 调用都在可下断点的 .cpp 文件中。
			static const char* exports[] = {
#include "api_exports.inc"
			};
			for (const char* name : exports) {
				Resolve<void*>(name);
			}
			// C++ 虚表之外的辅助函数仍需 dlsym；不会静态链接任何 SDK 实现。
			InitCpp = Resolve<decltype(InitCpp)>("sovkit_init_cpp");
			Free = Resolve<decltype(Free)>("sovkit_free");
			ErrorName = Resolve<decltype(ErrorName)>("sovkit_error_name");
			ErrorString = Resolve<decltype(ErrorString)>("sovkit_error_string");
			StorageProtect = Resolve<decltype(StorageProtect)>("sovkit_storage_protect");
			StorageUnprotect = Resolve<decltype(StorageUnprotect)>("sovkit_storage_unprotect");
			VaultCreate = Resolve<decltype(VaultCreate)>("sovkit_vault_create");
			VaultUnlock = Resolve<decltype(VaultUnlock)>("sovkit_vault_unlock");
			StoreDeriveKey = Resolve<decltype(StoreDeriveKey)>("sovkit_store_derive_key");
			StoreOpenV1 = Resolve<decltype(StoreOpenV1)>("sovkit_store_open_v1");
			StoreCloseV1 = Resolve<decltype(StoreCloseV1)>("sovkit_store_close_v1");
			LogConfigureV1 = Resolve<decltype(LogConfigureV1)>("sovkit_log_configure_v1");
			LogReadV1 = Resolve<decltype(LogReadV1)>("sovkit_log_read_v1");
			LogEmitV1 = Resolve<decltype(LogEmitV1)>("sovkit_log_emit_v1");
			LogStatsV1 = Resolve<decltype(LogStatsV1)>("sovkit_log_stats_v1");
			StoreQueryV1 = Resolve<decltype(StoreQueryV1)>("sovkit_store_query_v1");
			StoreConversationV1 = Resolve<decltype(StoreConversationV1)>("sovkit_store_conversation_v1");
			StoreResultV1 = Resolve<decltype(StoreResultV1)>("sovkit_store_result_v1");
			BleTransport = Resolve<decltype(BleTransport)>("sovkit_ble_transport");
			NetworkPathList = Resolve<decltype(NetworkPathList)>("sovkit_network_path_list");
			NetworkPathsUpdate = Resolve<decltype(NetworkPathsUpdate)>("sovkit_network_paths_update");
			MessageForget = Resolve<decltype(MessageForget)>("sovkit_message_forget");
			NetworkShare = Resolve<decltype(NetworkShare)>("sovkit_network_share");
			ConnectionAssistance = Resolve<decltype(ConnectionAssistance)>("sovkit_connection_assistance");
			GetLastError = Resolve<decltype(GetLastError)>("sovkit_get_last_error");
		}
		catch (...) {
			// 此时尚未取得进程级 C++ 句柄，可以卸载失败的库并让用户重新选择。
			Close();
			throw;
		}
	}

	void SdkLibrary::Close() {
		if (module_ && !retained_) {
			dlclose(module_);
		}
		// 取得 ISovKit 后保留模块直到进程退出，避免留下失效的进程级借用句柄。
		module_ = nullptr;
	}

	SdkLibrary::~SdkLibrary() {
		Close();
	}

}
