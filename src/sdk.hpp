#pragma once
#include <sovkit.h>
#include <nlohmann/json.hpp>
#include <array>
#include <filesystem>
#include <functional>
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
			return library_ != nullptr;
		}
		bool started() const {
			return started_;
		}
		static constexpr size_t symbol_count =
#include "api_count.inc"
		    ;

	private:
#include "api_members.inc"
		template <class T>
		T symbol(const char* name);
		Json capture(const std::function<int(char**, size_t*)>& call, char*& out, size_t& size);
		Json output(const std::function<int(char**, size_t*)>& call);
		void persist();
		void lock_profile(const fs::path& profile);
		void unlock_profile();
		static int SOVKIT_CALL read(void*, const char*, uint8_t*, size_t*);
		static int SOVKIT_CALL write(void*, const char*, const uint8_t*, size_t);
		static int SOVKIT_CALL remove(void*, const char*);
		static void SOVKIT_CALL log(uint64_t, sovkit_log_level_t, const char*, size_t, void*);
		void* library_ = nullptr;
		bool started_ = false, registered_ = false, store_open_ = false;
		fs::path profile_;
		std::array<uint8_t, 32> key_{};
		std::map<std::string, std::vector<uint8_t>> records_;
		intptr_t lock_ = -1;
		std::mutex log_mutex_;
		std::deque<std::string> logs_;
	};
}
