#pragma once
#include <sovkit.h>
#include <cstddef>
#include <cstdint>

namespace devtools {

	// SDK 分配的返回缓冲区必须由同一个 SDK 释放，不能使用 delete/free。
	class SdkBuffer {
	public:
		explicit SdkBuffer(decltype(&sovkit_free) release, bool sensitive = false)
		    : release_(release), sensitive_(sensitive) {
		}
		~SdkBuffer() {
			// 密钥明文用完先擦除；异常退出也会执行这里。
			if (sensitive_) {
				volatile auto* bytes = reinterpret_cast<volatile uint8_t*>(data);
				for (size_t index = 0; bytes && index < size; ++index) {
					bytes[index] = 0;
				}
			}
			if (data) {
				release_(data);
			}
		}
		SdkBuffer(const SdkBuffer&) = delete;
		SdkBuffer& operator=(const SdkBuffer&) = delete;

		// 保留原始指针和字节长度，方便在 SDK 调用前后观察变量。
		char* data = nullptr;
		size_t size = 0;

	private:
		decltype(&sovkit_free) release_;
		bool sensitive_;
	};

}
