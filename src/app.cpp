#include "catalogue.hpp"
#include <libwxui.hpp>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <mutex>
#include <thread>
#include "banner.hpp"

namespace devtools {
struct Job { std::string op; Json request; };
class Worker {
public:
  explicit Worker(std::function<void(Json)> deliver) : deliver_(std::move(deliver)), thread_([this] { run(); }) {}
  ~Worker() { { std::lock_guard lock(mutex_); quit_ = true; } cv_.notify_one(); thread_.join(); }
  bool submit(Job job) {
    std::lock_guard lock(mutex_);
    if (jobs_.size() >= 16 || quit_) return false;
    jobs_.push_back(std::move(job)); cv_.notify_one(); return true;
  }
private:
  void post(Json record) { deliver_(std::move(record)); }
  void run() {
    Sdk sdk;
    uint64_t sequence = 0;
    for (;;) {
      Job job;
      {
        std::unique_lock lock(mutex_);
        cv_.wait_for(lock, std::chrono::milliseconds(250), [&] { return quit_ || !jobs_.empty(); });
        if (quit_ && jobs_.empty()) break;
        if (jobs_.empty()) job.op = "events";
        else { job = std::move(jobs_.front()); jobs_.pop_front(); }
      }
      const auto began = std::chrono::steady_clock::now();
      if (sdk.loaded()) {
        auto logs = sdk.logs();
        if (!logs.empty()) post({{"operation", "logs"}, {"data", logs}});
      }
      Json result;
      try {
        if (job.op == "load") {
          sdk.load(path_from_utf8(job.request.at("path")));
          result = sdk.execute("info"); result["symbols"] = Sdk::symbol_count;
        } else if (job.op == "start") {
          auto password = std::move(job.request["password"].get_ref<std::string &>()); job.request.erase("password");
          struct Guard { std::string &s; ~Guard() { wipe(s.data(), s.size()); } } guard{password};
          result = sdk.start(path_from_utf8(job.request.value("profile", "")), password, job.request.at("deviceName"));
        } else if (job.op == "events") {
          if (!sdk.started()) continue;
          auto events = sdk.events();
          if (events.empty()) continue;
          result = {{"code", 0}, {"data", events}};
        } else result = sdk.execute(job.op, job.request);
      } catch (const std::exception &e) { result = {{"code", -1}, {"message", e.what()}}; }
      result["operation"] = job.op; result["sequence"] = ++sequence;
      result["elapsedMs"] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - began).count();
      post(std::move(result));
    }
  }
  std::function<void(Json)> deliver_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<Job> jobs_;
  bool quit_ = false;
  std::thread thread_;
};

class Frame {
public:
  explicit Frame(fs::path smoke = {})
      : window_({"SovKit DevTools · 独立 SDK 调试工作台", {1260, 860}, {980, 700}, 2}, kWorkbench),
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
    bind("browseSdk", [this] {
      if (auto path = window_.OpenFile("选择与本机架构匹配的 libsovkit",
          "Dynamic library (*.dylib;*.so;*.dll)|*.dylib;*.so;*.dll|All files|*")) library_->SetValueUtf8(*path);
    });
    bind("load", [this] { enqueue("load", {{"path", library_->GetValueUtf8()}}); });
    bind("docs", [this] { window_.ShowText("SDK 接入文档", kSdkDocs); });
    bind("browseProfile", [this] {
      if (auto path = window_.ChooseDirectory("选择空目录或已有 DevTools 目录（不使用 Nearvia 数据）")) profile_->SetValueUtf8(*path);
    });
    bind("start", [this] {
      enqueue("start", {{"deviceName", device_->GetValueUtf8()}, {"profile", profile_->GetValueUtf8()}, {"password", password_->GetValueUtf8()}});
      password_->Clear();
    });
    bind("stop", [this] { enqueue("stop"); });
    bind("execute", [this] { execute(); });
    bind("reset", [this] { request_->SetValueUtf8(recipe(selected_).request.dump(2)); });
    bind("sendFile", [this] {
      auto path = window_.OpenFile("选择测试发送文件");
      if (!path) return;
      auto data = recipe("transfer_offer").request;
      try { auto current = Json::parse(request_->GetValueUtf8()); if (current.contains("relationshipId")) data["relationshipId"] = current["relationshipId"]; } catch (...) {}
      data["sourcePaths"] = {*path}; data["logicalNames"] = {utf8(path_from_utf8(*path).filename())};
      select("transfer_offer"); request_->SetValueUtf8(data.dump(2));
    });
    bind("receiveDirectory", [this] {
      auto path = window_.ChooseDirectory("明确授权保存接收文件的目录");
      if (!path) return;
      auto data = recipe("transfer_decide").request;
      try { auto current = Json::parse(request_->GetValueUtf8()); if (current.contains("transferId")) data["transferId"] = current["transferId"]; } catch (...) {}
      data["destinationDirectory"] = *path;
      select("transfer_decide"); request_->SetValueUtf8(data.dump(2));
    });
    bind("clear", [this] { events_->Clear(); history_->Clear(); logs_->Clear(); response_->SetJson(""); diagnostics_.clear(); });
    bind("export", [this] { export_report(); });
    for (const auto* page : {"events", "history", "logs"}) {
      bind(std::string(page) + "Tab", [this, page] { window_.Require<wxui::TabLayout>("records")->SelectItem(page); });
    }
    tree_->Bind("itemselect", [this](const wxui::NotifyEvent& event) {
      auto* node = tree_->GetVisibleNode(event.param1);
      if (node && !node->GetUserData().empty()) select(node->GetUserData());
    });
    auto* search = window_.Require<wxui::Edit>("search");
    search->Bind("valuechanged", [this, search](const wxui::NotifyEvent&) { populate(search->GetValueUtf8()); });
    window_.OnClose([this](bool canDefer) {
      if (!canDefer) { worker_.reset(); return true; }
      if (!closing_) {
        closing_ = true;
        if (enqueue("stop")) window_.SetStatus("正在停止 SDK、提交数据…");
      }
      return false;
    });
    auto post = window_.Poster();
    worker_ = std::make_unique<Worker>([this, post](Json row) {
      post([this, row = std::move(row)]() mutable { result(std::move(row)); });
    });
    const auto exe = path_from_utf8(wxui::ExecutablePath());
#ifdef _WIN32
    auto bundled = exe.parent_path() / "libsovkit.dll";
#elif defined(__APPLE__)
    auto bundled = exe.parent_path().parent_path() / "Frameworks/libsovkit.dylib";
#else
    auto bundled = exe.parent_path() / "libsovkit.so";
#endif
    if (fs::exists(bundled)) library_->SetValueUtf8(utf8(bundled));
    populate(""); select("info");
    window_.SetStatus("未加载 SDK · 所有网络操作由你手动触发");
    if (!smoke_.empty()) post([this] { enqueue("load", {{"path", library_->GetValueUtf8()}}); });
  }
  void show() { window_.Present(); }
private:
  template<class F> void bind(const std::string& name, F callback) {
    window_.Require<wxui::Control>(name)->Bind("click", [callback](const wxui::NotifyEvent&) { callback(); });
  }
  void populate(const std::string& filter) {
    tree_->RemoveAllRoots();
    std::map<std::string, std::shared_ptr<wxui::TreeNode>> groups;
    std::vector<std::shared_ptr<wxui::TreeNode>> ordered;
    std::vector<std::string> names{"selftest"}; for (auto x : operations()) names.push_back(x.name);
    for (const auto& op : names) {
      auto r = recipe(op);
      if (!filter.empty() && op.find(filter) == std::string::npos && r.title.find(filter) == std::string::npos && r.group.find(filter) == std::string::npos) continue;
      auto& group = groups[r.group];
      if (!group) { group = std::make_shared<wxui::TreeNode>(); group->SetText(r.group); ordered.push_back(group); }
      auto node = std::make_shared<wxui::TreeNode>();
      node->SetText(r.title); node->SetAttribute("userdata", op);
      group->AddTreeChild(node);
    }
    for (auto& group : ordered) tree_->AddRoot(group);
    tree_->ExpandAll();
  }
  void select(const std::string& op) {
    selected_ = op; auto r = recipe(op);
    title_->SetText(r.title + "  /  " + op); help_->SetValueUtf8(r.help); request_->SetValueUtf8(r.request.dump(2));
  }
  bool enqueue(std::string op, Json request = Json::object()) {
    if (!worker_->submit({std::move(op), std::move(request)})) {
      closing_ = false; window_.SetStatus("请求队列已满，请等待当前操作完成"); return false;
    }
    execute_->SetEnabled(false); start_->SetEnabled(false); load_->SetEnabled(false); ++pending_;
    return true;
  }
  void execute() {
    try {
      auto r = recipe(selected_); auto data = Json::parse(request_->GetValueUtf8());
      if (r.confirm && !window_.Confirm("确认手动操作", "将执行 " + selected_ + "。请核对请求、授权和目标。")) return;
      enqueue(selected_, data);
    } catch (...) { window_.Error("请求格式", "JSON 无效，未调用 SDK。"); }
  }
  void append(wxui::RichEdit* view, const std::string& text) { view->AppendBounded(text + "\n", 200000); }
  void result(Json row) {
    const auto op = row.value("operation", "");
    if (op == "events") { append(events_, row.dump(2)); return; }
    if (op == "logs") { append(logs_, row["data"].dump(2)); return; }
    if (pending_) --pending_;
    if (op == "load" && row.value("code", -1) == 0) loaded_ = true;
    execute_->SetEnabled(!closing_ && pending_ == 0); start_->SetEnabled(!closing_ && pending_ == 0); load_->SetEnabled(!loaded_ && !closing_ && pending_ == 0);
    response_->SetJson(row.dump());
    append(history_, op + "  code=" + std::to_string(row.value("code", -1)) + "  " + std::to_string(row.value("elapsedMs", 0)) + " ms");
    diagnostics_.push_back(diagnostic(row)); if (diagnostics_.size() > 500) diagnostics_.pop_front();
    window_.SetStatus(op + (row.value("code", -1) == 0 ? " · 完成，请查看业务状态" : " · 失败，请查看响应"));
    if (!smoke_.empty() && op == "load" && row.value("code", -1) == 0) {
      window_.SetStatus("SDK " + row["data"].value("sdkVersion", "unknown"), 1);
      select("selftest"); enqueue("selftest");
    } else if (!smoke_.empty() && (op == "selftest" || op == "load")) {
      window_.RefreshLayout();
      Json report = diagnostic(row);
      const auto size = window_.ClientExtent();
      report["clientWidth"] = size.width; report["clientHeight"] = size.height;
      report["uiBackend"] = "libwxui";
      report["responseViewer"] = "libwxui::JsonViewer";
      report["responseJsonMatches"] = Json::parse(response_->GetRawJson()) == row;
      report["requestWidth"] = request_->GetWidth(); report["responseWidth"] = response_->GetWidth();
      std::ofstream output(smoke_, std::ios::binary); output << report.dump(2) << '\n';
      // The smoke runner closes this window after inspecting the rendered UI.
    }
    if (closing_ && op == "stop") {
      if (row.value("code", -1) == 0) { worker_.reset(); window_.FinishClose(); }
      else {
        closing_ = false; execute_->SetEnabled(pending_ == 0); start_->SetEnabled(pending_ == 0);
        load_->SetEnabled(!loaded_ && pending_ == 0);
        window_.Error("保留测试数据", "SDK 尚未安全停止。请保留窗口，检查响应后重试停止。");
      }
    }
  }
  void export_report() {
    auto path = window_.SaveFile("导出操作名、状态码、耗时；不含请求、消息、身份、路径或密码", "sovkit-diagnostics.json", "JSON|*.json");
    if (!path) return;
    std::ofstream out(path_from_utf8(*path), std::ios::binary);
    Json report{{"format", 1}, {"toolVersion", "0.1.0"}, {"records", diagnostics_}};
    out << report.dump(2) << '\n'; out.close();
    window_.SetStatus(out ? "诊断元数据已导出" : "导出失败");
  }
  // Destroy/join the worker before the window and its callback dispatcher.
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
      for (size_t i = 1; i + 1 < args.size(); ++i) if (args[i] == "--smoke-report") smoke = path_from_utf8(args[++i]);
      frame_ = std::make_unique<Frame>(smoke); frame_->show(); return true;
    } catch (const std::exception& e) { wxui::ShowError("SovKit DevTools", e.what()); return false; }
  }
  int OnAppExit() override { frame_.reset(); return 0; }
  std::unique_ptr<Frame> frame_;
};
}
WXUI_IMPLEMENT_APPLICATION(devtools::App);
