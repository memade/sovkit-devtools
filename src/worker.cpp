#include "worker.hpp"
#include <chrono>

namespace devtools {

	Worker::Worker(std::function<void(Json)> deliver) : deliver_(std::move(deliver)) {
		thread_ = std::thread(&Worker::run, this); // 状态成员初始化完成后才启动线程。
	}

	Worker::~Worker() {
		{
			std::lock_guard lock(mutex_);
			quit_ = true;
		}
		cv_.notify_one(); // 唤醒空闲线程，让它处理完已提交的任务再退出。
		thread_.join();
	}

	bool Worker::submit(Job job) {
		std::lock_guard lock(mutex_);
		if (jobs_.size() >= 16 || quit_) {
			return false;
		}
		jobs_.push_back(std::move(job));
		cv_.notify_one();
		return true;
	}

	bool Worker::TakeJob(Job& job) {
		std::unique_lock lock(mutex_);
		cv_.wait_for(lock, std::chrono::milliseconds(250), [this] { return quit_ || !jobs_.empty(); });
		if (quit_ && jobs_.empty()) {
			return false;
		}
		if (jobs_.empty()) {
			job.op = "events"; // 没有用户请求时轮询事件，未启动的 SDK 会跳过。
		}
		else {
			job = std::move(jobs_.front());
			jobs_.pop_front();
		}
		return true;
	}

	Json Worker::ProcessJob(Sdk& sdk, Job& job) {
		if (job.op == "load") {
			// 在这里检查用户最终选中的 DLL 路径，再进入 Sdk::load。
			const fs::path path = path_from_utf8(job.request.at("path"));
			sdk.load(path);
			Json result = sdk.execute("info");
			result["symbols"] = Sdk::symbol_count;
			return result;
		}
		if (job.op == "start") {
			// 将口令从请求中取走，避免进入操作记录；异常路径也清除口令。
			std::string password = std::move(job.request["password"].get_ref<std::string&>());
			job.request.erase("password");
			struct PasswordGuard {
				std::string& value;
				~PasswordGuard() {
					wipe(value.data(), value.size());
				}
			} guard{password};
			const fs::path profile = path_from_utf8(job.request.value("profile", ""));
			const std::string device = job.request.at("deviceName");
			return sdk.start(profile, password, device);
		}
		if (job.op == "events") {
			if (!sdk.started()) {
				return nullptr;
			}
			Json events = sdk.events();
			if (events.empty()) {
				return nullptr;
			}
			return {{"code", 0}, {"data", events}};
		}
		// 普通 API 请求进入具名路由，下一步即可跳到对应操作的 .cpp 文件。
		return sdk.execute(job.op, job.request);
	}

	void Worker::post(Json record) {
		deliver_(std::move(record)); // 调用方负责投递到 UI 线程，这里不访问控件。
	}

	void Worker::run() {
		Sdk sdk; // 在调试器中展开 sdk.handle_ 可查看当前 ISovKit 对象。
		uint64_t sequence = 0;
		for (;;) {
			Job job; // 请求只存活于本轮处理，空闲时不保留上一次消息内容。
			if (!TakeJob(job)) {
				break;
			}
			const auto began = std::chrono::steady_clock::now();
			if (sdk.loaded()) {
				Json logs = sdk.logs();
				if (!logs.empty()) {
					post({{"operation", "logs"}, {"data", logs}});
				}
			}
			Json result;
			try {
				result = ProcessJob(sdk, job);
			}
			catch (const std::exception& error) {
				result = {{"code", -1}, {"message", error.what()}};
			}
			if (result.is_null()) {
				continue;
			} // 空闲轮询不产生空白操作记录。
			result["operation"] = job.op;
			result["sequence"] = ++sequence;
			result["elapsedMs"] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - began).count();
			post(std::move(result));
		}
		// sdk 在本线程析构；等待 SDK 停止并解除回调后，线程才真正退出。
	}

}
