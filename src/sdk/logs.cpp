#include "../sdk.hpp"

namespace devtools {
	// SDK 日志回调只入队，界面线程稍后再读取。
	void SOVKIT_CALL Sdk::log(uint64_t, sovkit_log_level_t, const char* data, size_t size, void* user) {
		if (!user || !data || size > 32768)
			return;
		try {
			auto* self = static_cast<Sdk*>(user);
			std::lock_guard lock(self->log_mutex_);
			if (self->logs_.size() >= 256)
				self->logs_.pop_front();
			self->logs_.emplace_back(data, size);
		}
		catch (...) {
		} // 不能让 C++ 异常跨越 SDK 的 C 回调边界。
	}
	Json Sdk::logs() {
		Json rows = Json::array();
		std::lock_guard lock(log_mutex_);
		for (auto& line : logs_) {
			auto item = Json::parse(line, nullptr, false);
			if (!item.is_discarded())
				rows.push_back(std::move(item));
		}
		logs_.clear();
		return rows;
	}
}
