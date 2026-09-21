#include "catalogue.hpp"
#include "worker.hpp"
#include <libwxui.hpp>
#include <fstream>
#include "assets.hpp"

namespace devtools {
	class Frame {
	public:
		explicit Frame(fs::path smoke = {})
		    : window_({"SovKit DevTools · 独立 SDK 调试工作台", {1260, 860}, {980, 700}, 2}, assets::Get("workbench.xml"), assets::Load),
		      smoke_(std::move(smoke)) {
			library_ = window_.Require<wxui::Edit>("library");
			device_ = window_.Require<wxui::Edit>("device");
			profile_ = window_.Require<wxui::Edit>("profile");
			password_ = window_.Require<wxui::Edit>("password");
			request_ = window_.Require<wxui::RichEdit>("request");
			help_ = window_.Require<wxui::RichEdit>("help");
			events_ = window_.Require<wxui::RichEdit>("events");
			history_ = window_.Require<wxui::RichEdit>("history");
			logs_ = window_.Require<wxui::RichEdit>("logs");
			response_ = window_.Require<wxui::JsonViewer>("response");
			title_ = window_.Require<wxui::Label>("title");
			tree_ = window_.Require<wxui::TreeView>("apiTree");
			load_ = window_.Require<wxui::Button>("load");
			start_ = window_.Require<wxui::Button>("start");
			execute_ = window_.Require<wxui::Button>("execute");
			device_->SetValueUtf8("DevTools-" + wxui::HostName());
			response_->SetJson("响应（本机可见；不会自动写入文件）");
			BindActions(); // 注册界面动作；每个 SDK 入口都有独立处理函数。
			auto post = window_.Poster();
			// SDK 后台结果先投递到 UI 线程，再更新响应和日志控件。
			worker_ = std::make_unique<Worker>([this, post](Json row) { post([this, row = std::move(row)]() mutable { result(std::move(row)); }); });
			// 仅预填可执行文件旁的 SDK 路径，构造窗口时不加载动态库。
			const auto exe = path_from_utf8(wxui::ExecutablePath());
#ifdef _WIN32
			auto bundled = exe.parent_path() / "libsovkit.dll";
#elif defined(__APPLE__)
			auto bundled = exe.parent_path() / "libsovkit.dylib";
#else
			auto bundled = exe.parent_path() / "libsovkit.so";
#endif
			library_->SetValueUtf8(utf8(bundled));
			populate("");
			select("info");
			start_->SetEnabled(false);
			execute_->SetEnabled(false);
			const bool available = fs::is_regular_file(bundled);
			window_.SetStatus(available ? "SDK 尚未加载，请点击“加载并检查”" : "SDK 尚未加载，请选择动态库后点击“加载并检查”");
		}
		void show() {
			window_.Present();
		}

	private:
		void OnChooseSdk() {
			// 调试时选择与 PDB 同次构建、同目录的 DLL；此步骤不会加载它。
			const auto path = window_.OpenFile("选择与本机架构匹配的 libsovkit",
			                                   "Dynamic library (*.dylib;*.so;*.dll)|*.dylib;*.so;*.dll|All files|*");
			if (path) {
				library_->SetValueUtf8(*path);
			}
		}
		void OnLoadSdk() {
			// 断点入口 1：确认路径；下一站是 Worker::ProcessJob，再进入 Sdk::load。
			const std::string path = library_->GetValueUtf8();
			enqueue("load", {{"path", path}});
		}
		void OnStartIdentity() {
			// 加载与启动分开：只有本按钮会创建或打开独立测试身份。
			Json request{{"deviceName", device_->GetValueUtf8()},
			             {"profile", profile_->GetValueUtf8()},
			             {"password", password_->GetValueUtf8()}};
			enqueue("start", std::move(request));
			password_->Clear(); // 提交后立即清空界面中的口令。
		}
		void OnChooseProfile() {
			const auto path = window_.ChooseDirectory("选择空目录或已有 DevTools 目录（不使用 Nearvia 数据）");
			if (path) {
				profile_->SetValueUtf8(*path);
			}
		}
		void OnChooseSendFile() {
			const auto path = window_.OpenFile("选择测试发送文件");
			if (!path) {
				return;
			}
			Json data = recipe("transfer_offer").request;
			try {
				// 保留用户已填的关系 ID，选择文件本身不发起传输。
				const Json current = Json::parse(request_->GetValueUtf8());
				if (current.contains("relationshipId")) {
					data["relationshipId"] = current["relationshipId"];
				}
			}
			catch (const Json::exception&) {
				// 原请求尚未写完时，从默认模板重新填写。
			}
			data["sourcePaths"] = {*path};
			data["logicalNames"] = {utf8(path_from_utf8(*path).filename())};
			select("transfer_offer");
			request_->SetValueUtf8(data.dump(2));
		}
		void OnChooseReceiveDirectory() {
			const auto path = window_.ChooseDirectory("明确授权保存接收文件的目录");
			if (!path) {
				return;
			}
			Json data = recipe("transfer_decide").request;
			try {
				// 保留待接收的传输 ID，实际接收仍由执行按钮触发。
				const Json current = Json::parse(request_->GetValueUtf8());
				if (current.contains("transferId")) {
					data["transferId"] = current["transferId"];
				}
			}
			catch (const Json::exception&) {
				// 原请求不是完整 JSON 时，使用默认模板。
			}
			data["destinationDirectory"] = *path;
			select("transfer_decide");
			request_->SetValueUtf8(data.dump(2));
		}
		void OnClearRecords() {
			events_->Clear();
			history_->Clear();
			logs_->Clear();
			response_->SetJson("");
			diagnostics_.clear();
		}
		void OnSelectApi(const wxui::NotifyEvent& event) {
			auto* node = tree_->GetVisibleNode(event.param1);
			if (node && !node->GetUserData().empty()) {
				select(node->GetUserData()); // 叶节点保存 API 名称，分类节点不执行操作。
			}
		}
		bool OnClose(bool canDefer) {
			if (!canDefer) {
				worker_.reset(); // 必须等 SDK 停止和后台线程退出，才能销毁窗口。
				return true;
			}
			if (!closing_) {
				closing_ = true;
				if (enqueue("stop")) {
					window_.SetStatus("正在停止 SDK、提交数据…");
				}
			}
			return false; // 正常关闭等待 result 收到 stop 成功后调用 FinishClose。
		}
		void BindActions() {
			// 这里只绑定事件；需要调试的动作放在对应的具名处理函数中。
			bind("browseSdk", [this] { OnChooseSdk(); });
			bind("load", [this] { OnLoadSdk(); });
			bind("docs", [this] { window_.ShowText("SovKit SDK 接入文档（随包原文）", assets::Get("SDK_INTEGRATION.md")); });
			bind("notices", [this] { window_.ShowText("关于与许可", assets::Notices()); });
			bind("browseProfile", [this] { OnChooseProfile(); });
			bind("start", [this] { OnStartIdentity(); });
			bind("stop", [this] { enqueue("stop"); });
			bind("execute", [this] { execute(); });
			bind("reset", [this] { request_->SetValueUtf8(recipe(selected_).request.dump(2)); });
			bind("sendFile", [this] { OnChooseSendFile(); });
			bind("receiveDirectory", [this] { OnChooseReceiveDirectory(); });
			bind("clear", [this] { OnClearRecords(); });
			bind("export", [this] { export_report(); });
			for (const auto* page : {"events", "history", "logs"}) {
				bind(std::string(page) + "Tab", [this, page] { window_.Require<wxui::TabLayout>("records")->SelectItem(page); });
			}
			tree_->Bind("itemselect", [this](const wxui::NotifyEvent& event) { OnSelectApi(event); });
			auto* search = window_.Require<wxui::Edit>("search");
			search->Bind("valuechanged", [this, search](const wxui::NotifyEvent&) { populate(search->GetValueUtf8()); });
			window_.OnClose([this](bool canDefer) { return OnClose(canDefer); });
		}
		template <class F>
		void bind(const std::string& name, F callback) {
			window_.Require<wxui::Control>(name)->Bind("click", [callback](const wxui::NotifyEvent&) { callback(); });
		}
		void populate(const std::string& filter) {
			tree_->RemoveAllRoots();
			std::map<std::string, std::shared_ptr<wxui::TreeNode>> groups;
			std::vector<std::shared_ptr<wxui::TreeNode>> ordered;
			std::vector<std::string> names{"selftest"};
			for (auto x : operations())
				names.push_back(x.name);
			for (const auto& op : names) {
				auto r = recipe(op);
				if (!filter.empty() && op.find(filter) == std::string::npos && r.title.find(filter) == std::string::npos && r.group.find(filter) == std::string::npos)
					continue;
				auto& group = groups[r.group];
				if (!group) {
					group = std::make_shared<wxui::TreeNode>();
					group->SetText(r.group);
					ordered.push_back(group);
				}
				auto node = std::make_shared<wxui::TreeNode>();
				node->SetText(r.title);
				node->SetAttribute("userdata", op);
				group->AddTreeChild(node);
			}
			for (auto& group : ordered)
				tree_->AddRoot(group);
			tree_->ExpandAll();
		}
		void select(const std::string& op) {
			selected_ = op;
			auto r = recipe(op);
			title_->SetText(r.title + "  /  " + op);
			help_->SetValueUtf8(r.help);
			request_->SetValueUtf8(r.request.dump(2));
		}
		bool enqueue(std::string op, Json request = Json::object()) {
			if (!worker_->submit({std::move(op), std::move(request)})) {
				closing_ = false;
				window_.SetStatus("请求队列已满，请等待当前操作完成");
				return false;
			}
			execute_->SetEnabled(false);
			start_->SetEnabled(false);
			load_->SetEnabled(false);
			++pending_;
			return true;
		}
		void execute() {
			try {
				auto r = recipe(selected_);
				auto data = Json::parse(request_->GetValueUtf8());
				// 用户确认后只提交任务，SDK 调用在 Worker 线程中执行。
				if (r.confirm && !window_.Confirm("确认手动操作", "将执行 " + selected_ + "。请核对请求、授权和目标。"))
					return;
				enqueue(selected_, data);
			}
			catch (...) {
				window_.Error("请求格式", "JSON 无效，未调用 SDK。");
			}
		}
		void append(wxui::RichEdit* view, const std::string& text) {
			view->AppendBounded(text + "\n", 200000);
		}
		void result(Json row) {
			const auto op = row.value("operation", "");
			if (op == "events") {
				append(events_, row.dump(2));
				return;
			}
			if (op == "logs") {
				append(logs_, row["data"].dump(2));
				return;
			}
			if (pending_)
				--pending_;
			// 只有加载和能力检查成功后，才开放身份与业务操作。
			if (op == "load" && row.value("code", -1) == 0) {
				loaded_ = true;
				window_.SetStatus("SDK " + row["data"].value("sdkVersion", "unknown"), 1);
			}
			execute_->SetEnabled(loaded_ && !closing_ && pending_ == 0);
			start_->SetEnabled(loaded_ && !closing_ && pending_ == 0);
			load_->SetEnabled(!loaded_ && !closing_ && pending_ == 0);
			response_->SetJson(row.dump());
			append(history_, op + "  code=" + std::to_string(row.value("code", -1)) + "  " + std::to_string(row.value("elapsedMs", 0)) + " ms");
			diagnostics_.push_back(diagnostic(row));
			if (diagnostics_.size() > 500)
				diagnostics_.pop_front();
			window_.SetStatus(op + (row.value("code", -1) == 0 ? " · 完成，请查看业务状态" : " · 失败，请查看响应"));
			if (!smoke_.empty() && op == "load" && row.value("code", -1) == 0) {
				select("selftest");
				enqueue("selftest");
			}
			else if (!smoke_.empty() && (op == "selftest" || op == "load")) {
				window_.RefreshLayout();
				Json report = diagnostic(row);
				const auto size = window_.ClientExtent();
				report["clientWidth"] = size.width;
				report["clientHeight"] = size.height;
				report["uiBackend"] = "libwxui";
				report["responseViewer"] = "libwxui::JsonViewer";
				report["responseJsonMatches"] = Json::parse(response_->GetRawJson()) == row;
				report["requestWidth"] = request_->GetWidth();
				report["responseWidth"] = response_->GetWidth();
				std::ofstream output(smoke_, std::ios::binary);
				output << report.dump(2) << '\n';
				// 冒烟检查读取报告并检查界面后，再关闭测试窗口。
			}
			if (closing_ && op == "stop") {
				if (row.value("code", -1) == 0) {
					worker_.reset();
					window_.FinishClose();
				}
				else {
					closing_ = false;
					execute_->SetEnabled(loaded_ && pending_ == 0);
					start_->SetEnabled(loaded_ && pending_ == 0);
					load_->SetEnabled(!loaded_ && pending_ == 0);
					window_.Error("保留测试数据", "SDK 尚未安全停止。请保留窗口，检查响应后重试停止。");
				}
			}
		}
		void export_report() {
			auto path = window_.SaveFile("导出操作名、状态码、耗时；不含请求、消息、身份、路径或密码", "sovkit-diagnostics.json", "JSON|*.json");
			if (!path)
				return;
			std::ofstream out(path_from_utf8(*path), std::ios::binary);
			Json report{{"format", 1}, {"toolVersion", "0.1.0"}, {"records", diagnostics_}};
			out << report.dump(2) << '\n';
			out.close();
			window_.SetStatus(out ? "诊断元数据已导出" : "导出失败");
		}
		// 成员逆序析构：先停止 worker_，再释放窗口及其回调派发器。
		wxui::DesktopWindow window_;
		wxui::Edit *library_, *device_, *profile_, *password_;
		wxui::RichEdit *request_, *help_, *events_, *history_, *logs_;
		wxui::JsonViewer* response_;
		wxui::Label* title_;
		wxui::TreeView* tree_;
		wxui::Button *load_, *start_, *execute_;
		std::string selected_;
		bool closing_ = false, loaded_ = false;
		size_t pending_ = 0;
		std::deque<Json> diagnostics_;
		fs::path smoke_;
		std::unique_ptr<Worker> worker_;
	};
	class App : public wxui::Application {
		bool OnAppInit() override {
			SetName("SovKit DevTools");
			try {
				fs::path smoke;
				const auto args = Arguments();
				for (size_t i = 1; i + 1 < args.size(); ++i)
					if (args[i] == "--smoke-report")
						smoke = path_from_utf8(args[++i]);
				frame_ = std::make_unique<Frame>(smoke);
				frame_->show();
				return true;
			}
			catch (const std::exception& e) {
				wxui::ShowError("SovKit DevTools", e.what());
				return false;
			}
		}
		int OnAppExit() override {
			frame_.reset();
			return 0;
		}
		std::unique_ptr<Frame> frame_;
	};
}
WXUI_IMPLEMENT_APPLICATION(devtools::App);
