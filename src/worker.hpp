#pragma once
#include "sdk.hpp"
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

namespace devtools {

	struct Job {
		std::string op;
		Json request;
	};

	// 界面只提交 Job；所有 SDK 调用在同一条后台线程中串行执行。
	class Worker {
	public:
		explicit Worker(std::function<void(Json)> deliver);
		~Worker();
		bool submit(Job job);

	private:
		bool TakeJob(Job& job);
		Json ProcessJob(Sdk& sdk, Job& job);
		void run();
		void post(Json record);
		std::function<void(Json)> deliver_;
		std::mutex mutex_;
		std::condition_variable cv_;
		std::deque<Job> jobs_;
		bool quit_ = false;
		std::thread thread_;
	};

}
