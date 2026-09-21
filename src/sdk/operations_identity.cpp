#include "../sdk.hpp"

namespace devtools {

	Json Sdk::Info() {
		SdkBuffer output(library_.Free);
		// 读取运行时版本和能力。
		const int code = handle_->Info(&output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::IdentityCreate(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 创建测试身份。
		const int code = handle_->IdentityCreate(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::IdentityOpen() {
		SdkBuffer output(library_.Free);
		// 打开已有测试身份。
		const int code = handle_->IdentityOpen(&output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::IdentityRotateDevice(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 轮换当前设备身份。
		const int code = handle_->IdentityRotateDevice(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::IdentityRenameDevice(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 修改设备名称。
		const int code = handle_->IdentityRenameDevice(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::IdentityRevokeActiveDevice(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 撤销当前设备。
		const int code = handle_->IdentityRevokeActiveDevice(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::IdentityListDevices() {
		SdkBuffer output(library_.Free);
		// 列出身份下的设备。
		const int code = handle_->IdentityListDevices(&output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::IdentitySign(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 使用身份对输入签名。
		const int code = handle_->IdentitySign(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::IdentityVerify(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 验证身份签名。
		const int code = handle_->IdentityVerify(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::IdentityExport(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 导出 SDK 规定格式的身份数据。
		const int code = handle_->IdentityExport(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::IdentityImport(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 导入 SDK 规定格式的身份数据。
		const int code = handle_->IdentityImport(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::GetLastError() {
		SdkBuffer output(library_.Free);
		// 读取 SDK 最近错误码（非虚 C 辅助入口）。
		const int code = library_.GetLastError();
		return MakeResponse(code, output);
	}

}
