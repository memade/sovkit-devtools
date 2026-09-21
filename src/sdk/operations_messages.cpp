#include "../sdk.hpp"

namespace devtools {

	Json Sdk::MessageSend(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 向指定关系发送消息。
		const int code = handle_->MessageSend(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::MessageList(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 读取会话消息。
		const int code = handle_->MessageList(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::MessageFlush() {
		SdkBuffer output(library_.Free);
		// 刷新消息队列。
		const int code = handle_->MessageFlush();
		return MakeResponse(code, output);
	}

	Json Sdk::ConversationExport(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 导出指定会话。
		const int code = handle_->ConversationExport(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::TransferOffer(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 提交文件发送请求。
		const int code = handle_->TransferOffer(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::TransferDecide(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 明确接受或拒绝接收文件。
		const int code = handle_->TransferDecide(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::TransferCancel(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 取消指定文件传输。
		const int code = handle_->TransferCancel(input.data(), input.size());
		return MakeResponse(code, output);
	}

	Json Sdk::TransferPause(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 暂停指定文件传输。
		const int code = handle_->TransferPause(input.data(), input.size());
		return MakeResponse(code, output);
	}

	Json Sdk::TransferResume(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 恢复指定文件传输。
		const int code = handle_->TransferResume(input.data(), input.size());
		return MakeResponse(code, output);
	}

	Json Sdk::TransferForget(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 移除指定传输记录。
		const int code = handle_->TransferForget(input.data(), input.size());
		return MakeResponse(code, output);
	}

	Json Sdk::TransferList(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 列出传输记录。
		const int code = handle_->TransferList(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::TransferFlush() {
		SdkBuffer output(library_.Free);
		// 刷新传输队列。
		const int code = handle_->TransferFlush();
		return MakeResponse(code, output);
	}

	Json Sdk::MessageForget(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 移除指定消息记录（非虚 C 辅助入口）。
		const int code = library_.MessageForget(input.data(), input.size());
		return MakeResponse(code, output);
	}

}
