#include "sdk.hpp"
#include <chrono>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#else
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace devtools {
std::string utf8(const fs::path &p) { auto s = p.u8string(); return {s.begin(), s.end()}; }
fs::path path_from_utf8(const std::string &s) { return fs::path(std::u8string(s.begin(), s.end())); }
void wipe(void *p, size_t n) { volatile auto *v = static_cast<volatile uint8_t *>(p); while (n--) *v++ = 0; }
const std::vector<ApiEntry> &operations() {
  static const std::vector<ApiEntry> result = {
#include "api_names.inc"
  };
  return result;
}
// Export metadata only. Arbitrary SDK/user strings are deliberately excluded.
Json diagnostic(const Json &r) {
  Json result = Json::object();
  for (const char *key : {"sequence", "elapsedMs", "code", "abiVersion", "symbols"})
    if (r.contains(key) && r[key].is_number()) result[key] = r[key];
  if (r.contains("operation") && r["operation"].is_string()) {
    const auto op = r["operation"].get<std::string>();
    if (op == "load" || op == "start" || op == "stop" || op == "events" || op == "selftest" ||
        std::any_of(operations().begin(), operations().end(), [&](auto x) { return op == x.name; }))
      result["operation"] = op;
  }
  result["redaction"] = "metadata-only-v1";
  return result;
}
namespace {
#ifdef _WIN32
// Like chmod(0700) on macOS: protect only an explicitly selected empty or
// DevTools-owned directory. Never take ownership of another user's directory.
class ProfilePermissions {
  struct LocalMemory { void *value = nullptr; ~LocalMemory() { if (value) LocalFree(value); } } descriptor_;
  struct Handle { HANDLE value = INVALID_HANDLE_VALUE; ~Handle() { if (value != INVALID_HANDLE_VALUE && value) CloseHandle(value); } };
  std::vector<unsigned char> user_;
  PSID sid() const { return reinterpret_cast<const TOKEN_USER *>(user_.data())->User.Sid; }
  static void require(bool ok) {
    if (!ok) throw std::runtime_error("Cannot prepare private Windows profile (error " + std::to_string(GetLastError()) + ")");
  }
public:
  ProfilePermissions() {
    Handle token;
    require(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token.value) != FALSE);
    DWORD size = 0;
    GetTokenInformation(token.value, TokenUser, nullptr, 0, &size);
    require(size != 0); user_.resize(size);
    require(GetTokenInformation(token.value, TokenUser, user_.data(), size, &size) != FALSE);
    LocalMemory text;
    require(ConvertSidToStringSidW(sid(), reinterpret_cast<LPWSTR *>(&text.value)) != FALSE);
    const auto *name = static_cast<const wchar_t *>(text.value);
    const std::wstring sddl = std::wstring(L"O:") + name + L"D:P(A;OICI;FA;;;" + name + L")(A;OICI;FA;;;SY)";
    require(ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &descriptor_.value, nullptr) != FALSE);
  }
  void create(const fs::path &path) {
    fs::create_directories(path.parent_path());
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), descriptor_.value, FALSE};
    if (!CreateDirectoryW(path.c_str(), &attributes)) require(GetLastError() == ERROR_ALREADY_EXISTS);
  }
  void protect(const fs::path &path) {
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
    PACL acl = nullptr; BOOL present = FALSE, defaulted = FALSE;
    require(GetSecurityDescriptorDacl(descriptor_.value, &present, &acl, &defaulted) && present);
    const auto changed = SetSecurityInfo(directory.value, SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
        nullptr, nullptr, acl, nullptr);
    if (changed != ERROR_SUCCESS)
      throw std::runtime_error("Cannot protect profile ACL (Windows error " + std::to_string(changed) + ")");
  }
};
#endif
std::vector<uint8_t> bytes(const fs::path &path) {
  const auto size = fs::file_size(path);
  if (size > 64 * 1024 * 1024) throw std::runtime_error("Profile file exceeds 64 MiB");
  std::ifstream in(path, std::ios::binary);
  std::vector<uint8_t> data(static_cast<size_t>(size));
  if (!in.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(size)))
    throw std::runtime_error("Cannot read profile file");
  return data;
}
void atomic_write(const fs::path &path, const void *data, size_t size) {
  auto temporary = path; temporary += ".pending";
#ifdef _WIN32
  HANDLE f = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (f == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create profile transaction; retain .pending for inspection");
  DWORD written = 0;
  bool ok = size <= MAXDWORD && WriteFile(f, data, static_cast<DWORD>(size), &written, nullptr) && written == size && FlushFileBuffers(f);
  CloseHandle(f);
  if (!ok || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    throw std::runtime_error("Profile commit failed; original data retained");
#else
  int fd = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
  if (fd < 0) throw std::runtime_error("Cannot create profile transaction; retain .pending for inspection");
  size_t offset = 0;
  while (offset < size) {
    auto n = ::write(fd, static_cast<const char *>(data) + offset, size - offset);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) { ::close(fd); throw std::runtime_error("Profile write failed"); }
    offset += static_cast<size_t>(n);
  }
  bool ok = fsync(fd) == 0; ::close(fd);
  if (!ok || ::rename(temporary.c_str(), path.c_str()) != 0) throw std::runtime_error("Profile commit failed");
  int dir = ::open(path.parent_path().c_str(), O_RDONLY);
  if (dir < 0) throw std::runtime_error("Cannot sync profile directory");
  ok = fsync(dir) == 0; ::close(dir);
  if (!ok) throw std::runtime_error("Profile directory sync failed");
#endif
}
}
template <class T> T Sdk::symbol(const char *name) {
#ifdef _WIN32
  auto ptr = GetProcAddress(static_cast<HMODULE>(library_), name);
#else
  auto ptr = dlsym(library_, name);
#endif
  if (!ptr) throw std::runtime_error(std::string("Missing public symbol: ") + name);
  return reinterpret_cast<T>(ptr);
}
void Sdk::load(const fs::path &path) {
  if (loaded()) throw std::runtime_error("Restart the tool to switch SDK binaries");
  if (!path.is_absolute() || !fs::is_regular_file(path)) throw std::runtime_error("Select an absolute SDK library file");
#ifdef _WIN32
  library_ = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
#else
  library_ = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
  if (!library_) {
#ifdef _WIN32
    const auto detail = "Windows error " + std::to_string(GetLastError()) +
        " (126: missing DLL/dependency; 193: wrong architecture or invalid image)";
#else
    const char *error = dlerror();
    const std::string detail = error ? error : "unknown loader error";
#endif
    throw std::runtime_error("Cannot load SDK: " + detail);
  }
  try {
    sovkit_abi_version = symbol<decltype(sovkit_abi_version)>("sovkit_abi_version");
    if (sovkit_abi_version() != SOVKIT_ABI_VERSION) throw std::runtime_error("SDK/header ABI mismatch: runtime=" + std::to_string(sovkit_abi_version()) + ", header=" + std::to_string(SOVKIT_ABI_VERSION));
#include "api_load.inc"
    if (sovkit_register_log_cb(log, this) != 0) throw std::runtime_error("Cannot register log callback");
  } catch (...) {
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(library_));
#else
    dlclose(library_);
#endif
    library_ = nullptr; throw;
  }
}
Json Sdk::capture(const std::function<int(char **, size_t *)> &call, char *&out, size_t &size) {
  const int code = call(&out, &size);
  struct Release { char *data; decltype(sovkit_free) free; ~Release() { if (data) free(data); } } release{out, sovkit_free};
  Json data = nullptr;
  if (out && size) data = Json::parse(out, out + size); // exact length, always freed on parse failure
  return {{"code", code}, {"name", sovkit_error_name(code)}, {"message", sovkit_error_string(code)}, {"data", data}};
}
Json Sdk::output(const std::function<int(char **, size_t *)> &f) { char *out = nullptr; size_t size = 0; return capture(f, out, size); }
Json Sdk::execute(const std::string &op, const Json &request) {
  if (op == "stop") return stop();
  if (!loaded()) throw std::runtime_error("Load SDK first");
  if (!request.is_object()) throw std::runtime_error("Request must be a JSON object");
  const std::string input = request.dump();
  if (input.size() > 1024 * 1024) throw std::runtime_error("Request exceeds 1 MiB");
  if (op == "events") return events();
  if (op == "selftest") {
    std::array<uint8_t, 32> test_key{};
    const uint8_t plain[] = {0, 42, 0, 255};
    char *encrypted = nullptr, *decoded = nullptr; size_t encrypted_n = 0, decoded_n = 0;
    int code = sovkit_storage_protect(test_key.data(), plain, sizeof(plain), &encrypted, &encrypted_n);
    if (code == 0) code = sovkit_storage_unprotect(test_key.data(), reinterpret_cast<uint8_t *>(encrypted), encrypted_n, &decoded, &decoded_n);
    const bool same = code == 0 && decoded_n == sizeof(plain) && std::memcmp(decoded, plain, sizeof(plain)) == 0;
    sovkit_free(encrypted); sovkit_free(decoded);
    return {{"code", same ? 0 : -1}, {"abiVersion", sovkit_abi_version()}, {"symbols", symbol_count}, {"data", {{"binaryRoundTrip", same}}}};
  }
  char *out = nullptr; size_t size = 0;
#include "api_dispatch.inc"
  throw std::runtime_error("Unsupported operation; lifecycle, callbacks and raw key pointers are managed by the host");
}
void Sdk::lock_profile(const fs::path &profile) {
  if (profile.empty()) return;
  if (!profile.is_absolute()) throw std::runtime_error("Profile directory must be absolute");
#ifdef _WIN32
  ProfilePermissions permissions;
  permissions.create(profile);
#else
  fs::create_directories(profile);
#endif
  profile_ = fs::canonical(profile);
  if (!fs::exists(profile_ / "devtools-profile.json") && !fs::is_empty(profile_)) {
    profile_.clear();
    throw std::runtime_error("Choose an empty directory or an existing DevTools profile; product data is never opened");
  }
#ifdef _WIN32
  permissions.protect(profile_);
#else
  if (::chmod(profile_.c_str(), 0700) != 0) throw std::runtime_error("Cannot protect profile directory");
#endif
  const auto file = profile_ / ".devtools.lock";
#ifdef _WIN32
  auto handle = CreateFileW(file.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (handle == INVALID_HANDLE_VALUE) throw std::runtime_error("Profile is already open or inaccessible");
  lock_ = reinterpret_cast<intptr_t>(handle);
#else
  int fd = ::open(file.c_str(), O_RDWR | O_CREAT | O_NOFOLLOW, 0600);
  if (fd < 0 || flock(fd, LOCK_EX | LOCK_NB) != 0) { if (fd >= 0) ::close(fd); throw std::runtime_error("Profile is already open or inaccessible"); }
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
  for (auto &[name, data] : records_) wipe(data.data(), data.size());
  records_.clear(); wipe(key_.data(), key_.size()); profile_.clear();
}
void Sdk::persist() {
  if (profile_.empty()) return;
  Json root = records_; std::string text = root.dump();
  char *out = nullptr; size_t size = 0;
  int code = sovkit_storage_protect(key_.data(), reinterpret_cast<const uint8_t *>(text.data()), text.size(), &out, &size);
  wipe(text.data(), text.size());
  if (code != 0) { sovkit_free(out); throw std::runtime_error("Cannot encrypt keystore"); }
  try { atomic_write(profile_ / "keystore.enc", out, size); } catch (...) { sovkit_free(out); throw; }
  sovkit_free(out);
}
int SOVKIT_CALL Sdk::read(void *user, const char *name, uint8_t *out, size_t *size) {
  if (!user || !name || !size) return -10000;
  try {
  auto &records = static_cast<Sdk *>(user)->records_;
  auto it = records.find(name); if (it == records.end()) return -10004;
  const size_t required = it->second.size(), capacity = *size; *size = required;
  if (!out) return 0; // Size query is success, not BUFFER_TOO_SMALL.
  if (capacity < required) return -10007;
  if (required) std::memcpy(out, it->second.data(), required);
  return 0;
  } catch (...) { return -10008; }
}
int SOVKIT_CALL Sdk::write(void *user, const char *name, const uint8_t *data, size_t size) {
  if (!user || !name || (!data && size)) return -10000;
  auto *self = static_cast<Sdk *>(user);
  try {
    auto prior = self->records_;
    self->records_[name] = size ? std::vector<uint8_t>(data, data + size) : std::vector<uint8_t>{};
    try { self->persist(); } catch (...) { self->records_ = std::move(prior); throw; }
    for (auto &[key, value] : prior) wipe(value.data(), value.size());
    return 0;
  } catch (...) { return -10008; }
}
int SOVKIT_CALL Sdk::remove(void *user, const char *name) {
  if (!user || !name) return -10000;
  auto *self = static_cast<Sdk *>(user);
  try { auto prior = self->records_; self->records_.erase(name);
    try { self->persist(); } catch (...) { self->records_ = std::move(prior); throw; }
    for (auto &[key, value] : prior) wipe(value.data(), value.size());
    return 0;
  } catch (...) { return -10008; }
}
Json Sdk::start(const fs::path &profile, std::string password, const std::string &device) {
  struct PasswordGuard { std::string &text; ~PasswordGuard() { wipe(text.data(), text.size()); } } guard{password};
  if (!loaded() || started_ || registered_ || store_open_) throw std::runtime_error("Load SDK / stop current session before starting");
  if (execute("info")["data"].value("keystoreDetachVersion", 0) < 1)
    throw std::runtime_error("This SDK lacks safe keystore detach. Use the updated 0.1.0 SDK with keystoreDetachVersion=1");
  if (device.empty() || device.size() > 64) throw std::runtime_error("Device name must be 1..64 UTF-8 bytes");
  try {
    lock_profile(profile);
    if (!profile_.empty()) {
      if (password.empty()) throw std::runtime_error("A persistent test profile requires a password");
      const auto envelope = profile_ / "vault.enc";
      if (fs::exists(envelope)) {
        auto data = bytes(envelope);
        if (sovkit_vault_unlock(reinterpret_cast<const uint8_t *>(password.data()), password.size(), data.data(), data.size(), key_.data()) != 0)
          throw std::runtime_error("Wrong password or damaged profile; original files retained");
        if (fs::exists(profile_ / "keystore.enc")) {
          auto encrypted = bytes(profile_ / "keystore.enc"); char *plain = nullptr; size_t n = 0;
          if (sovkit_storage_unprotect(key_.data(), encrypted.data(), encrypted.size(), &plain, &n) != 0) throw std::runtime_error("Damaged encrypted keystore");
          try { records_ = Json::parse(plain, plain + n).get<decltype(records_)>(); }
          catch (...) { wipe(plain, n); sovkit_free(plain); throw; }
          wipe(plain, n); sovkit_free(plain);
        } else throw std::runtime_error("Missing keystore; incomplete profile retained for inspection");
      } else {
        if (fs::exists(profile_ / "keystore.enc") || fs::exists(profile_ / "sovkit.db")) throw std::runtime_error("Missing envelope; refusing to replace existing identity");
        char *data = nullptr; size_t n = 0;
        int code = sovkit_vault_create(reinterpret_cast<const uint8_t *>(password.data()), password.size(), &data, &n, key_.data());
        if (code != 0) throw std::runtime_error("Cannot create password envelope");
        try { atomic_write(envelope, data, n); } catch (...) { sovkit_free(data); throw; }
        sovkit_free(data); persist();
      }
    }
    if (sovkit_register_keystore(read, write, remove, this) != 0) throw std::runtime_error("Cannot register keystore");
    registered_ = true;
    if (!profile_.empty()) {
      std::array<uint8_t, 32> database_key{};
      if (sovkit_store_derive_key(key_.data(), database_key.data()) != 0) throw std::runtime_error("Cannot derive database key");
      const auto request = Json{{"version", 1}, {"path", utf8(profile_ / "sovkit.db")}, {"deviceName", device}, {"create", true}}.dump();
      auto opened = output([&](char **out, size_t *n) { return sovkit_store_open_v1(request.data(), request.size(), database_key.data(), database_key.size(), out, n); });
      wipe(database_key.data(), database_key.size());
      if (opened["code"] != 0) throw std::runtime_error("Cannot submit encrypted store open: " + opened.dump());
      store_open_ = true;
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
      Json result;
      do {
        result = execute("store_result_v1", {{"requestId", opened["data"]["requestId"]}});
        if (result["code"] != -10004) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      } while (std::chrono::steady_clock::now() < deadline);
      if (result["code"] != 0 || result["data"].value("status", -1) != 0) throw std::runtime_error("Encrypted store open failed or timed out: " + result.dump());
    }
    const std::string empty = "{}"; sovkit_handle_t handle = nullptr;
    auto init = output([&](char **out, size_t *n) { return sovkit_init(&handle, empty.data(), empty.size(), out, n); });
    if (init["code"] != 0) throw std::runtime_error("SDK init failed: " + init.dump());
    started_ = true;
    auto identity = execute("identity_open");
    if (identity["code"] == -10004) identity = execute("identity_create", {{"deviceName", device}});
    if (identity["code"] != 0) throw std::runtime_error("Identity could not be opened: " + identity.dump());
    return {{"code", 0}, {"data", {{"identity", identity["data"]}, {"persistence", profile.empty() ? "memory-only" : "encrypted-keystore-and-SQLCipher"}, {"discovery", "stopped"}}}};
  } catch (...) {
    auto original = std::current_exception();
    auto result = stop();
    if (result["code"] != 0) throw std::runtime_error("Startup and shutdown failed; keep tool open and retry Stop");
    std::rethrow_exception(original);
  }
}
Json Sdk::stop() {
  if (!loaded()) return {{"code", 0}};
  int code = sovkit_shutdown();
  if (code != 0) return {{"code", code}, {"message", "Shutdown pending; preserve session and retry Stop"}};
  started_ = false;
  code = sovkit_store_close_v1();
  if (code != 0) return {{"code", code}, {"message", "Store close failed; preserve profile and retry Stop"}};
  store_open_ = false;
  if (registered_) {
    code = sovkit_register_keystore(nullptr, nullptr, nullptr, nullptr);
    if (code != 0) return {{"code", code}, {"message", "Cannot detach keystore"}};
    registered_ = false;
  }
  unlock_profile(); return {{"code", 0}, {"data", {{"stopped", true}}}};
}
Json Sdk::events() {
  Json rows = Json::array();
  if (!loaded()) return rows;
  for (int i = 0; i < 64; ++i) {
    auto result = output([&](char **out, size_t *n) { return sovkit_event_poll(out, n); });
    if (result["code"] == -10004) break;
    rows.push_back(result);
    if (result["code"] != 0) break;
  }
  return rows;
}
void SOVKIT_CALL Sdk::log(uint64_t, sovkit_log_level_t, const char *data, size_t size, void *user) {
  if (!user || !data || size > 32768) return;
  try {
    auto *self = static_cast<Sdk *>(user);
    std::lock_guard lock(self->log_mutex_);
    if (self->logs_.size() >= 256) self->logs_.pop_front();
    self->logs_.emplace_back(data, size);
  } catch (...) {} // Never propagate through a C callback.
}
Json Sdk::logs() {
  Json rows = Json::array(); std::lock_guard lock(log_mutex_);
  for (auto &line : logs_) {
    auto item = Json::parse(line, nullptr, false);
    if (!item.is_discarded()) rows.push_back(std::move(item));
  }
  logs_.clear(); return rows;
}
Sdk::~Sdk() {
  // Dynamic code remains mapped until process exit; no unload under SDK workers.
  // Normal GUI close checks stop first and keeps the window on failure.
  // During host teardown retain callback storage until a pending commit succeeds.
  if (!loaded()) return;
  for (;;) {
    try {
      if (stop().value("code", -1) == 0) break;
    } catch (...) {}
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }
  sovkit_register_log_cb(nullptr, nullptr);
}
}
