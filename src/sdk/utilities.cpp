#include "../sdk.hpp"
#include <algorithm>

namespace devtools {
	std::string utf8(const fs::path& p) {
		auto s = p.u8string();
		return {s.begin(), s.end()};
	}
	fs::path path_from_utf8(const std::string& s) {
		return fs::path(std::u8string(s.begin(), s.end()));
	}
	void wipe(void* p, size_t n) {
		volatile auto* v = static_cast<volatile uint8_t*>(p);
		while (n--)
			*v++ = 0;
	}
	const std::vector<ApiEntry>& operations() {
		static const std::vector<ApiEntry> result = {
#include "api_names.inc"
		};
		return result;
	}
	// 导出只保留操作名和状态等元数据，不写出请求、消息、密码或路径。
	Json diagnostic(const Json& r) {
		Json result = Json::object();
		for (const char* key : {"sequence", "elapsedMs", "code", "abiVersion", "symbols"})
			if (r.contains(key) && r[key].is_number())
				result[key] = r[key];
		if (r.contains("operation") && r["operation"].is_string()) {
			const auto op = r["operation"].get<std::string>();
			if (op == "load" || op == "start" || op == "stop" || op == "events" || op == "selftest" ||
			    std::any_of(operations().begin(), operations().end(), [&](auto x) { return op == x.name; }))
				result["operation"] = op;
		}
		result["redaction"] = "metadata-only-v1";
		return result;
	}
}
