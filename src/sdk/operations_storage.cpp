#include "../sdk.hpp"

namespace devtools {

	Json Sdk::LogConfigureV1(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 设置 SDK 日志规则（非虚 C 辅助入口）。
		const int code = library_.LogConfigureV1(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::LogReadV1(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 读取 SDK 保存的日志（非虚 C 辅助入口）。
		const int code = library_.LogReadV1(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::LogEmitV1(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 提交一条测试日志（非虚 C 辅助入口）。
		const int code = library_.LogEmitV1(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::LogStatsV1() {
		SdkBuffer output(library_.Free);
		// 读取 SDK 日志统计（非虚 C 辅助入口）。
		const int code = library_.LogStatsV1(&output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::StoreQueryV1(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 提交加密数据库查询（非虚 C 辅助入口）。
		const int code = library_.StoreQueryV1(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::StoreConversationV1(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 查询加密数据库中的会话（非虚 C 辅助入口）。
		const int code = library_.StoreConversationV1(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::StoreResultV1(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 查询异步数据库任务的最终结果（非虚 C 辅助入口）。
		const int code = library_.StoreResultV1(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

}
