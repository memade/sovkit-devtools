#include "sdk.hpp"
#include <chrono>
#include <stdexcept>
#include <thread>

namespace devtools {

	void Sdk::load(const fs::path& path) {
		if (loaded()) {
			throw std::runtime_error("Restart the tool to switch SDK binaries");
		}
		// 这里只在用户点击加载后执行；构造 Sdk 不会加载任何库。
		library_.Open(path);
		try {
			// 获取公开 C++ 句柄，不启动身份、网络或后台 SDK 业务。
			sovkit_handle_t raw_handle = nullptr;
			const int code = library_.InitCpp(&raw_handle);
			if (code != 0 || !raw_handle) {
				throw std::runtime_error("Cannot obtain ISovKit handle: " + std::to_string(code));
			}
			handle_ = static_cast<ISovKit*>(raw_handle);
			library_.Retain(); // 句柄有效期为整个进程，不能卸载它所在的代码。
			// 从这里开始，虚接口都通过 handle_ 调用，可直接 F11 进入 SDK。
			const int log_code = handle_->RegisterLogCallback(log, this);
			if (log_code != 0) {
				throw std::runtime_error("Cannot register log callback: " + std::to_string(log_code));
			}
		}
		catch (...) {
			handle_ = nullptr;
			library_.Close();
			throw;
		}
	}

	Json Sdk::MakeResponse(int code, const SdkBuffer& buffer) const {
		Json data = nullptr;
		if (buffer.data && buffer.size) {
			// 使用 SDK 返回的精确字节数解析；SdkBuffer 在异常路径也会释放内存。
			data = Json::parse(buffer.data, buffer.data + buffer.size);
		}
		const char* name = library_.ErrorName(code);
		const char* message = library_.ErrorString(code);
		return {{"code", code}, {"name", name}, {"message", message}, {"data", data}};
	}

	Sdk::~Sdk() {
		if (!loaded()) {
			return;
		}
		// 先等待 SDK 停止，回调使用的成员必须保留到停止成功。
		for (;;) {
			try {
				if (stop().value("code", -1) == 0) {
					break;
				}
			}
			catch (...) {
				// 提交失败时保留密钥与回调，稍后重试，不能提前释放宿主对象。
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
		}
		handle_->RegisterLogCallback(nullptr, nullptr); // 解除最后一个指向 this 的回调。
		handle_ = nullptr;                              // 借用句柄不 delete；动态库保留到进程退出。
	}

}
