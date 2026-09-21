#include "../sdk.hpp"
#include <chrono>
#include <stdexcept>
#include <thread>

namespace devtools {

	Json Sdk::start(const fs::path& profile, std::string password, const std::string& device) {
		// 无论启动成功还是抛出异常，都清除这份口令副本。
		struct PasswordGuard {
			std::string& value;
			~PasswordGuard() {
				wipe(value.data(), value.size());
			}
		} password_guard{password};
		if (!loaded() || started_ || registered_ || store_open_) {
			throw std::runtime_error("Load SDK / stop current session before starting");
		}
		if (Info()["data"].value("keystoreDetachVersion", 0) < 1) {
			throw std::runtime_error("This SDK lacks safe keystore detach. Use the updated 0.1.0 SDK with keystoreDetachVersion=1");
		}
		if (device.empty() || device.size() > 64) {
			throw std::runtime_error("Device name must be 1..64 UTF-8 bytes");
		}
		try {
			PrepareProfile(profile, password); // 锁定测试目录，并创建或解锁宿主密钥库。
			const int registered = handle_->RegisterKeystore(read, write, remove, this);
			if (registered != 0) {
				throw std::runtime_error("Cannot register keystore");
			}
			registered_ = true; // 从此必须先解除 SDK 回调，才能释放 records_。
			OpenStore(device);  // 持久模式先打开加密数据库，临时模式跳过。

			const std::string request = "{}";
			SdkBuffer output(library_.Free);
			// 在这里下断点可直接进入 ISovKit::Init 的 SDK 实现。
			const int code = handle_->Init(request.data(), request.size(), &output.data, &output.size);
			if (code != 0) {
				throw std::runtime_error("SDK init failed: " + MakeResponse(code, output).dump());
			}
			started_ = true;

			Json identity = IdentityOpen(); // 优先恢复测试身份，只有不存在时才创建。
			if (identity["code"] == -10004) {
				const std::string create_request = Json{{"deviceName", device}}.dump();
				identity = IdentityCreate(create_request);
			}
			if (identity["code"] != 0) {
				throw std::runtime_error("Identity could not be opened: " + identity.dump());
			}
			return {{"code", 0}, {"data", {{"identity", identity["data"]}, {"persistence", profile.empty() ? "memory-only" : "encrypted-keystore-and-SQLCipher"}, {"discovery", "stopped"}}}};
		}
		catch (...) {
			// 保存原始失败原因，按正常停止顺序清理已完成的启动步骤。
			auto original = std::current_exception();
			const Json stopped = stop();
			if (stopped["code"] != 0) {
				throw std::runtime_error("Startup and shutdown failed; keep tool open and retry Stop");
			}
			std::rethrow_exception(original);
		}
	}

	void Sdk::OpenStore(const std::string& device) {
		if (profile_.empty()) {
			return;
		}
		std::array<uint8_t, 32> database_key{};
		struct KeyGuard {
			std::array<uint8_t, 32>& value;
			~KeyGuard() {
				wipe(value.data(), value.size());
			}
		} key_guard{database_key};
		// 数据库密钥派生规则由 SDK 定义，DevTools 只传入原始密钥。
		if (library_.StoreDeriveKey(key_.data(), database_key.data()) != 0) {
			throw std::runtime_error("Cannot derive database key");
		}
		const std::string input = Json{{"version", 1}, {"path", utf8(profile_ / "sovkit.db")}, {"deviceName", device}, {"create", true}}.dump();
		SdkBuffer output(library_.Free);
		const int code = library_.StoreOpenV1(input.data(), input.size(), database_key.data(), database_key.size(), &output.data, &output.size);
		wipe(database_key.data(), database_key.size()); // SDK 返回后立即清除派生密钥。
		if (code == 0) {
			store_open_ = true;
		}
		const Json opened = MakeResponse(code, output);
		if (code != 0) {
			throw std::runtime_error("Cannot submit encrypted store open: " + opened.dump());
		}
		// 提交成功不代表异步打开完成；按 SDK 返回的 requestId 查询最终状态。
		const std::string query = Json{{"requestId", opened["data"]["requestId"]}}.dump();
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
		Json result;
		do {
			result = StoreResultV1(query);
			if (result["code"] != -10004) {
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		} while (std::chrono::steady_clock::now() < deadline);
		if (result["code"] != 0 || result["data"].value("status", -1) != 0) {
			throw std::runtime_error("Encrypted store open failed or timed out: " + result.dump());
		}
	}

	Json Sdk::stop() {
		if (!loaded()) {
			return {{"code", 0}};
		}
		// 必须先等 SDK 的工作线程结束，再关闭数据库和解除宿主回调。
		int code = handle_->Shutdown();
		if (code != 0) {
			return {{"code", code}, {"message", "Shutdown pending; preserve session and retry Stop"}};
		}
		started_ = false;
		code = library_.StoreCloseV1();
		if (code != 0) {
			return {{"code", code}, {"message", "Store close failed; preserve profile and retry Stop"}};
		}
		store_open_ = false;
		if (registered_) {
			code = handle_->RegisterKeystore(nullptr, nullptr, nullptr, nullptr);
			if (code != 0) {
				return {{"code", code}, {"message", "Cannot detach keystore"}};
			}
			registered_ = false;
		}
		unlock_profile(); // 回调已解除，现在才能擦除密钥并释放目录锁。
		return {{"code", 0}, {"data", {{"stopped", true}}}};
	}

}
