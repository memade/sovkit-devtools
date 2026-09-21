#include "../sdk.hpp"
#include <cstring>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace devtools {
	namespace {
#ifdef _WIN32
		// 仅收紧当前用户明确选中的测试目录权限，不接管其他用户的目录。
		class ProfilePermissions {
			struct LocalMemory {
				void* value = nullptr;
				~LocalMemory() {
					if (value)
						LocalFree(value);
				}
			} descriptor_;
			struct Handle {
				HANDLE value = INVALID_HANDLE_VALUE;
				~Handle() {
					if (value != INVALID_HANDLE_VALUE && value)
						CloseHandle(value);
				}
			};
			std::vector<unsigned char> user_;
			PSID sid() const {
				return reinterpret_cast<const TOKEN_USER*>(user_.data())->User.Sid;
			}
			static void require(bool ok) {
				if (!ok)
					throw std::runtime_error("Cannot prepare private Windows profile (error " + std::to_string(GetLastError()) + ")");
			}

		public:
			ProfilePermissions() {
				Handle token;
				require(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token.value) != FALSE);
				DWORD size = 0;
				GetTokenInformation(token.value, TokenUser, nullptr, 0, &size);
				require(size != 0);
				user_.resize(size);
				require(GetTokenInformation(token.value, TokenUser, user_.data(), size, &size) != FALSE);
				LocalMemory text;
				require(ConvertSidToStringSidW(sid(), reinterpret_cast<LPWSTR*>(&text.value)) != FALSE);
				const auto* name = static_cast<const wchar_t*>(text.value);
				const std::wstring sddl = std::wstring(L"O:") + name + L"D:P(A;OICI;FA;;;" + name + L")(A;OICI;FA;;;SY)";
				require(ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &descriptor_.value, nullptr) != FALSE);
			}
			void create(const fs::path& path) {
				fs::create_directories(path.parent_path());
				SECURITY_ATTRIBUTES attributes{sizeof(attributes), descriptor_.value, FALSE};
				if (!CreateDirectoryW(path.c_str(), &attributes))
					require(GetLastError() == ERROR_ALREADY_EXISTS);
			}
			void protect(const fs::path& path) {
				Handle directory;
				directory.value = CreateFileW(path.c_str(), READ_CONTROL | WRITE_DAC,
				                              FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
				                              FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
				require(directory.value != INVALID_HANDLE_VALUE);
				BY_HANDLE_FILE_INFORMATION info{};
				require(GetFileInformationByHandle(directory.value, &info) != FALSE);
				if (!(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
					throw std::runtime_error("Profile must be a regular directory");
				LocalMemory existing;
				PSID owner = nullptr;
				const auto status = GetSecurityInfo(directory.value, SE_FILE_OBJECT,
				                                    OWNER_SECURITY_INFORMATION, &owner, nullptr, nullptr, nullptr, &existing.value);
				if (status != ERROR_SUCCESS || !owner || !EqualSid(owner, sid()))
					throw std::runtime_error("Choose a profile directory owned by the current Windows user");
				PACL acl = nullptr;
				BOOL present = FALSE, defaulted = FALSE;
				require(GetSecurityDescriptorDacl(descriptor_.value, &present, &acl, &defaulted) && present);
				const auto changed = SetSecurityInfo(directory.value, SE_FILE_OBJECT,
				                                     DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
				                                     nullptr, nullptr, acl, nullptr);
				if (changed != ERROR_SUCCESS)
					throw std::runtime_error("Cannot protect profile ACL (Windows error " + std::to_string(changed) + ")");
			}
		};
#endif
		std::vector<uint8_t> bytes(const fs::path& path) {
			// 限制测试配置文件大小，防止损坏文件导致超量分配。
			const auto size = fs::file_size(path);
			if (size > 64 * 1024 * 1024)
				throw std::runtime_error("Profile file exceeds 64 MiB");
			std::ifstream in(path, std::ios::binary);
			std::vector<uint8_t> data(static_cast<size_t>(size));
			if (!in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size)))
				throw std::runtime_error("Cannot read profile file");
			return data;
		}
		void atomic_write(const fs::path& path, const void* data, size_t size) {
			// 原文件只有在写入、同步全部成功后才被替换。
			auto temporary = path;
			temporary += ".pending";
#ifdef _WIN32
			HANDLE f = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (f == INVALID_HANDLE_VALUE)
				throw std::runtime_error("Cannot create profile transaction; retain .pending for inspection");
			DWORD written = 0;
			bool ok = size <= MAXDWORD && WriteFile(f, data, static_cast<DWORD>(size), &written, nullptr) && written == size && FlushFileBuffers(f);
			CloseHandle(f);
			if (!ok || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
				throw std::runtime_error("Profile commit failed; original data retained");
#else
			int fd = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
			if (fd < 0)
				throw std::runtime_error("Cannot create profile transaction; retain .pending for inspection");
			size_t offset = 0;
			while (offset < size) {
				auto n = ::write(fd, static_cast<const char*>(data) + offset, size - offset);
				if (n < 0 && errno == EINTR)
					continue;
				if (n <= 0) {
					::close(fd);
					throw std::runtime_error("Profile write failed");
				}
				offset += static_cast<size_t>(n);
			}
			bool ok = fsync(fd) == 0;
			::close(fd);
			if (!ok || ::rename(temporary.c_str(), path.c_str()) != 0)
				throw std::runtime_error("Profile commit failed");
			int dir = ::open(path.parent_path().c_str(), O_RDONLY);
			if (dir < 0)
				throw std::runtime_error("Cannot sync profile directory");
			ok = fsync(dir) == 0;
			::close(dir);
			if (!ok)
				throw std::runtime_error("Profile directory sync failed");
#endif
		}
	}
	void Sdk::lock_profile(const fs::path& profile) {
		if (profile.empty())
			return;
		if (!profile.is_absolute())
			throw std::runtime_error("Profile directory must be absolute");
#ifdef _WIN32
		ProfilePermissions permissions;
		permissions.create(profile);
#else
		fs::create_directories(profile);
#endif
		// 使用规范路径检查目录归属，并阻止同时打开同一个测试身份。
		profile_ = fs::canonical(profile);
		if (!fs::exists(profile_ / "devtools-profile.json") && !fs::is_empty(profile_)) {
			profile_.clear();
			throw std::runtime_error("Choose an empty directory or an existing DevTools profile; product data is never opened");
		}
#ifdef _WIN32
		permissions.protect(profile_);
#else
		if (::chmod(profile_.c_str(), 0700) != 0)
			throw std::runtime_error("Cannot protect profile directory");
#endif
		// 独占文件锁持续到 SDK 停止和回调解除之后。
		const auto file = profile_ / ".devtools.lock";
#ifdef _WIN32
		auto handle = CreateFileW(file.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (handle == INVALID_HANDLE_VALUE)
			throw std::runtime_error("Profile is already open or inaccessible");
		lock_ = reinterpret_cast<intptr_t>(handle);
#else
		int fd = ::open(file.c_str(), O_RDWR | O_CREAT | O_NOFOLLOW, 0600);
		if (fd < 0 || flock(fd, LOCK_EX | LOCK_NB) != 0) {
			if (fd >= 0)
				::close(fd);
			throw std::runtime_error("Profile is already open or inaccessible");
		}
		lock_ = fd;
#endif
		if (!fs::exists(profile_ / "devtools-profile.json")) {
			const std::string marker = "{\"format\":1,\"owner\":\"sovkit-devtools\"}\n";
			atomic_write(profile_ / "devtools-profile.json", marker.data(), marker.size());
		}
	}
	void Sdk::unlock_profile() {
		if (lock_ != -1) {
#ifdef _WIN32
			CloseHandle(reinterpret_cast<HANDLE>(lock_));
#else
			::close(static_cast<int>(lock_));
#endif
			lock_ = -1;
		}
		// 所有 SDK 回调已经解除，现在可以清空明文和主密钥。
		for (auto& [name, data] : records_)
			wipe(data.data(), data.size());
		records_.clear();
		wipe(key_.data(), key_.size());
		profile_.clear();
	}
	void Sdk::persist() {
		if (profile_.empty()) {
			return;
		}
		// SDK 回调持有的是明文记录；写盘前必须通过 SDK 加密。
		Json records = records_;
		std::string text = records.dump();
		struct PlaintextGuard {
			std::string& value;
			~PlaintextGuard() {
				wipe(value.data(), value.size());
			}
		} guard{text};
		SdkBuffer encrypted(library_.Free);
		const int code = library_.StorageProtect(key_.data(), reinterpret_cast<const uint8_t*>(text.data()), text.size(), &encrypted.data, &encrypted.size);
		if (code != 0) {
			throw std::runtime_error("Cannot encrypt keystore");
		}
		// 先写临时文件并同步，再替换旧文件；失败时保留原来的密钥库。
		atomic_write(profile_ / "keystore.enc", encrypted.data, encrypted.size);
	}
	int SOVKIT_CALL Sdk::read(void* user, const char* name, uint8_t* out, size_t* size) {
		if (!user || !name || !size)
			return -10000;
		try {
			auto& records = static_cast<Sdk*>(user)->records_;
			auto it = records.find(name);
			if (it == records.end())
				return -10004;
			const size_t required = it->second.size(), capacity = *size;
			*size = required;
			if (!out)
				return 0; // 空缓冲区表示查询长度，按 SDK 契约返回成功。
			if (capacity < required)
				return -10007;
			if (required)
				std::memcpy(out, it->second.data(), required);
			return 0;
		}
		catch (...) {
			return -10008;
		}
	}
	int SOVKIT_CALL Sdk::write(void* user, const char* name, const uint8_t* data, size_t size) {
		if (!user || !name || (!data && size))
			return -10000;
		auto* self = static_cast<Sdk*>(user);
		try {
			// 保留旧记录，写盘失败时回滚，不能向 SDK 报告虚假的成功。
			auto prior = self->records_;
			self->records_[name] = size ? std::vector<uint8_t>(data, data + size) : std::vector<uint8_t>{};
			try {
				self->persist();
			}
			catch (...) {
				// 同步提交失败，恢复原来的内存密钥库。
				self->records_ = std::move(prior);
				throw;
			}
			for (auto& [key, value] : prior)
				wipe(value.data(), value.size());
			return 0;
		}
		catch (...) {
			return -10008;
		}
	}
	int SOVKIT_CALL Sdk::remove(void* user, const char* name) {
		if (!user || !name)
			return -10000;
		auto* self = static_cast<Sdk*>(user);
		try {
			// 保留旧记录，写盘失败时回滚，不能向 SDK 报告虚假的成功。
			auto prior = self->records_;
			self->records_.erase(name);
			try {
				self->persist();
			}
			catch (...) {
				// 同步提交失败，恢复原来的内存密钥库。
				self->records_ = std::move(prior);
				throw;
			}
			for (auto& [key, value] : prior)
				wipe(value.data(), value.size());
			return 0;
		}
		catch (...) {
			return -10008;
		}
	}

	void Sdk::PrepareProfile(const fs::path& profile, const std::string& password) {
		lock_profile(profile); // 只允许空目录或已有 DevTools 测试目录，并取得独占锁。
		if (profile_.empty()) {
			return;
		} // 临时模式只在内存保存密钥库。
		if (password.empty()) {
			throw std::runtime_error("A persistent test profile requires a password");
		}
		const auto envelope = profile_ / "vault.enc";
		if (fs::exists(envelope)) {
			const auto data = bytes(envelope);
			// 口令只交给 SDK 解锁，不使用宿主自行实现的派生或解密算法。
			const int code = library_.VaultUnlock(reinterpret_cast<const uint8_t*>(password.data()), password.size(), data.data(), data.size(), key_.data());
			if (code != 0) {
				throw std::runtime_error("Wrong password or damaged profile; original files retained");
			}
			if (!fs::exists(profile_ / "keystore.enc")) {
				throw std::runtime_error("Missing keystore; incomplete profile retained for inspection");
			}
			const auto encrypted = bytes(profile_ / "keystore.enc");
			SdkBuffer plain(library_.Free, true); // 解析完成或抛出异常后都擦除明文。
			const int decoded = library_.StorageUnprotect(key_.data(), encrypted.data(), encrypted.size(), &plain.data, &plain.size);
			if (decoded != 0) {
				throw std::runtime_error("Damaged encrypted keystore");
			}
			records_ = Json::parse(plain.data, plain.data + plain.size).get<decltype(records_)>();
		}
		else {
			// 缺少口令封套但仍有身份文件时，不能创建新身份覆盖旧数据。
			if (fs::exists(profile_ / "keystore.enc") || fs::exists(profile_ / "sovkit.db")) {
				throw std::runtime_error("Missing envelope; refusing to replace existing identity");
			}
			SdkBuffer created(library_.Free);
			const int code = library_.VaultCreate(reinterpret_cast<const uint8_t*>(password.data()), password.size(), &created.data, &created.size, key_.data());
			if (code != 0) {
				throw std::runtime_error("Cannot create password envelope");
			}
			atomic_write(envelope, created.data, created.size);
			persist(); // 新封套落盘后，立即写入空密钥库，保证下次可以完整恢复。
		}
	}

}
