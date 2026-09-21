#pragma once
#include <sovkit.h>
#include "sdk/library.hpp"
#include "sdk/buffer.hpp"
#include <nlohmann/json.hpp>
#include <array>
#include <filesystem>
#include <map>
#include <mutex>
#include <deque>
#include <string>
#include <vector>

namespace devtools {
	using Json = nlohmann::json;
	namespace fs = std::filesystem;
	std::string utf8(const fs::path& path);
	fs::path path_from_utf8(const std::string& text);
	void wipe(void* data, size_t size);
	Json diagnostic(const Json& record);

	struct ApiEntry {
		const char* name;
		const char* shape;
	};
	const std::vector<ApiEntry>& operations();

	// SDK 调试入口：加载、初始化、业务调用、停止各自独立，便于设置函数断点。
	class Sdk {
	public:
		Sdk() = default;
		~Sdk();
		Sdk(const Sdk&) = delete;
		Sdk& operator=(const Sdk&) = delete;
		void load(const fs::path& library);
		Json execute(const std::string& operation, const Json& request = Json::object());
		Json start(const fs::path& profile, std::string password, const std::string& device);
		Json stop();
		Json events();
		Json logs();
		bool loaded() const {
			return handle_ != nullptr;
		}
		bool started() const {
			return started_;
		}
		// SDK 拥有此对象；这里只借用，禁止 delete。
		ISovKit* handle() const {
			return handle_;
		}
		static constexpr size_t symbol_count =
#include "api_count.inc"
		    ;

	private:
		// 每个方法都有独立实现，直接在对应 SDK 调用行设置断点。
		Json LogConfigureV1(const std::string& input);
		Json LogReadV1(const std::string& input);
		Json LogEmitV1(const std::string& input);
		Json LogStatsV1();
		Json StoreQueryV1(const std::string& input);
		Json StoreConversationV1(const std::string& input);
		Json StoreResultV1(const std::string& input);
		Json Info();
		Json TransportProbe(const std::string& input);
		Json IdentityCreate(const std::string& input);
		Json IdentityOpen();
		Json IdentityRotateDevice(const std::string& input);
		Json IdentityRenameDevice(const std::string& input);
		Json IdentityRevokeActiveDevice(const std::string& input);
		Json IdentityListDevices();
		Json IdentitySign(const std::string& input);
		Json IdentityVerify(const std::string& input);
		Json IdentityExport(const std::string& input);
		Json IdentityImport(const std::string& input);
		Json DiscoveryStart(const std::string& input);
		Json DiscoveryStop();
		Json DiscoveryStatus();
		Json DiscoveryList();
		Json ProximityInvitationCreate(const std::string& input);
		Json ProximityInvitationInspect(const std::string& input);
		Json PairingStartFromInvitation(const std::string& input);
		Json BleTransport(const std::string& input);
		Json PairingStart(const std::string& input);
		Json PairingStatus();
		Json SessionList();
		Json SessionResume(const std::string& input);
		Json WanOfferCreate(const std::string& input);
		Json WanOfferAccept(const std::string& input);
		Json WanAnswerAccept(const std::string& input);
		Json NetworkPathList();
		Json NetworkPathsUpdate(const std::string& input);
		Json WanRouteList();
		Json WanRouteStop(const std::string& input);
		Json PairingConfirm(const std::string& input);
		Json PairingCancel();
		Json RelationshipList();
		Json RelationshipRemove(const std::string& input);
		Json MessageSend(const std::string& input);
		Json MessageList(const std::string& input);
		Json MessageFlush();
		Json ConversationExport(const std::string& input);
		Json TransferOffer(const std::string& input);
		Json TransferDecide(const std::string& input);
		Json TransferCancel(const std::string& input);
		Json TransferPause(const std::string& input);
		Json TransferResume(const std::string& input);
		Json TransferForget(const std::string& input);
		Json TransferList(const std::string& input);
		Json TransferFlush();
		Json MessageForget(const std::string& input);
		Json NetworkShare(const std::string& input);
		Json ConnectionAssistance(const std::string& input);
		Json GetLastError();
		Json SelfTest();
		Json MakeResponse(int code, const SdkBuffer& buffer) const;
		void PrepareProfile(const fs::path& profile, const std::string& password);
		void OpenStore(const std::string& device);
		void persist();
		void lock_profile(const fs::path& profile);
		void unlock_profile();
		static int SOVKIT_CALL read(void*, const char*, uint8_t*, size_t*);
		static int SOVKIT_CALL write(void*, const char*, const uint8_t*, size_t);
		static int SOVKIT_CALL remove(void*, const char*);
		static void SOVKIT_CALL log(uint64_t, sovkit_log_level_t, const char*, size_t, void*);
		SdkLibrary library_;
		ISovKit* handle_ = nullptr; // 由 sovkit_init_cpp 返回的进程级借用句柄。
		bool started_ = false, registered_ = false, store_open_ = false;
		fs::path profile_;
		std::array<uint8_t, 32> key_{};
		std::map<std::string, std::vector<uint8_t>> records_;
		intptr_t lock_ = -1;
		std::mutex log_mutex_;
		std::deque<std::string> logs_;
	};
}
