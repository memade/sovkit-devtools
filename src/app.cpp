#include "catalogue.hpp"
#include <libwxui.hpp>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/notebook.h>
#include <wx/msgdlg.h>
#include <wx/splitter.h>
#include <wx/stdpaths.h>
#include <wx/stattext.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/treectrl.h>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <mutex>
#include <thread>
#include "banner.hpp"

namespace devtools {
wxString w(const std::string &s) { return wxString::FromUTF8(s.data(), s.size()); }
std::string u(const wxString &s) { return s.ToStdString(wxConvUTF8); }
wxDECLARE_EVENT(EVT_RESULT, wxThreadEvent);
wxDEFINE_EVENT(EVT_RESULT, wxThreadEvent);
struct Job { std::string op; Json request; };
class Worker {
public:
  explicit Worker(wxEvtHandler *target) : target_(target), thread_([this] { run(); }) {}
  ~Worker() { { std::lock_guard lock(mutex_); quit_ = true; } cv_.notify_one(); thread_.join(); }
  bool submit(Job job) {
    std::lock_guard lock(mutex_);
    if (jobs_.size() >= 16 || quit_) return false;
    jobs_.push_back(std::move(job)); cv_.notify_one(); return true;
  }
private:
  void post(Json record) { auto *e = new wxThreadEvent(EVT_RESULT); e->SetString(w(record.dump())); wxQueueEvent(target_, e); }
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
  wxEvtHandler *target_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<Job> jobs_;
  bool quit_ = false;
  std::thread thread_;
};
struct Item : wxTreeItemData { explicit Item(std::string name) : op(std::move(name)) {} std::string op; };

class Frame : public wxFrame {
public:
  explicit Frame(fs::path smoke = {}) : wxFrame(nullptr, wxID_ANY, w("SovKit DevTools · 独立 SDK 调试工作台"), wxDefaultPosition, wxSize(1260, 860)), smoke_(std::move(smoke)) {
    SetMinSize(wxSize(980, 700)); CreateStatusBar(2);
    auto *root = new wxBoxSizer(wxVERTICAL);
    wxui::ControlFactory::RegisterBuiltins();
    auto *banner = new wxui::UIManager(this);
    if (!banner->LoadFromString(std::string(kBanner))) throw std::runtime_error("Invalid embedded UI XML");
    root->Add(banner, 0, wxEXPAND); banner->SetMinSize(wxSize(-1, 72));
    auto *bar = new wxPanel(this); auto *bs = new wxBoxSizer(wxHORIZONTAL);
    library_ = new wxTextCtrl(bar, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
    bs->Add(new wxStaticText(bar, wxID_ANY, w("动态库")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 8);
    bs->Add(library_, 1, wxEXPAND | wxALL, 5);
    button(bar, bs, "选择 SDK…", [this] {
      wxFileDialog dlg(this, w("选择与本机架构匹配的 libsovkit"), "", "", "Dynamic library (*.dylib;*.so;*.dll)|*.dylib;*.so;*.dll|All files|*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
      if (dlg.ShowModal() == wxID_OK) library_->SetValue(dlg.GetPath());
    });
    load_ = button(bar, bs, "加载并检查", [this] { enqueue("load", {{"path", u(library_->GetValue())}}); });
    button(bar, bs, "接入文档", [this] { show_docs(); });
    bar->SetSizer(bs); root->Add(bar, 0, wxEXPAND);

    auto *session = new wxPanel(this); auto *ss = new wxBoxSizer(wxHORIZONTAL);
    device_ = new wxTextCtrl(session, wxID_ANY, "DevTools-" + wxGetHostName());
    profile_ = new wxTextCtrl(session, wxID_ANY, ""); profile_->SetHint(w("留空：临时内存身份"));
    password_ = new wxTextCtrl(session, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD); password_->SetHint(w("测试目录密码"));
    ss->Add(device_, 1, wxALL | wxEXPAND, 5); ss->Add(profile_, 2, wxALL | wxEXPAND, 5);
    button(session, ss, "测试目录…", [this] {
      wxDirDialog dlg(this, w("选择空目录或已有 DevTools 目录（不使用 Nearvia 数据）"));
      if (dlg.ShowModal() == wxID_OK) profile_->SetValue(dlg.GetPath());
    });
    ss->Add(password_, 1, wxALL | wxEXPAND, 5);
    start_ = button(session, ss, "启动身份", [this] {
      enqueue("start", {{"deviceName", u(device_->GetValue())}, {"profile", u(profile_->GetValue())}, {"password", u(password_->GetValue())}});
      password_->Clear();
    });
    stop_ = button(session, ss, "停止并保存", [this] { enqueue("stop"); });
    session->SetSizer(ss); root->Add(session, 0, wxEXPAND);

    auto *outer = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE | wxSP_3D);
    auto *nav = new wxPanel(outer); auto *ns = new wxBoxSizer(wxVERTICAL);
    auto *search = new wxTextCtrl(nav, wxID_ANY); search->SetHint(w("搜索 API / 中文功能")); ns->Add(search, 0, wxEXPAND | wxALL, 6);
    tree_ = new wxTreeCtrl(nav, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTR_HIDE_ROOT | wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT);
    ns->Add(tree_, 1, wxEXPAND | wxALL, 5); nav->SetSizer(ns);
    auto *workspace = new wxPanel(outer); auto *ws = new wxBoxSizer(wxVERTICAL);
    title_ = new wxStaticText(workspace, wxID_ANY, w("选择一个 API")); title_->SetFont(title_->GetFont().Bold().Larger());
    help_ = new wxTextCtrl(workspace, wxID_ANY, w("先加载 SDK，再启动一个独立测试身份。"), wxDefaultPosition, wxSize(-1, 65), wxTE_MULTILINE | wxTE_READONLY | wxBORDER_NONE);
    ws->Add(title_, 0, wxALL, 8); ws->Add(help_, 0, wxEXPAND | wxLEFT | wxRIGHT, 8);
    auto *actions = new wxBoxSizer(wxHORIZONTAL);
    execute_ = button(workspace, actions, "执行请求", [this] { execute(); });
    button(workspace, actions, "恢复模板", [this] { request_->SetValue(w(recipe(selected_).request.dump(2))); });
    button(workspace, actions, "选择发送文件…", [this] {
      wxFileDialog dlg(this, w("选择测试发送文件"), "", "", "All files|*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
      if (dlg.ShowModal() != wxID_OK) return;
      auto data = recipe("transfer_offer").request;
      try { auto current = Json::parse(u(request_->GetValue())); if (current.contains("relationshipId")) data["relationshipId"] = current["relationshipId"]; } catch (...) {}
      data["sourcePaths"] = {u(dlg.GetPath())}; data["logicalNames"] = {u(dlg.GetFilename())};
      select("transfer_offer"); request_->SetValue(w(data.dump(2)));
    });
    button(workspace, actions, "选择接收目录…", [this] {
      wxDirDialog dlg(this, w("明确授权保存接收文件的目录")); if (dlg.ShowModal() != wxID_OK) return;
      try { auto data = Json::parse(u(request_->GetValue())); data["destinationDirectory"] = u(dlg.GetPath()); request_->SetValue(w(data.dump(2))); }
      catch (...) { wxMessageBox(w("请先选择 transfer_decide 并使用有效 JSON。"), w("请求"), wxOK, this); }
    });
    ws->Add(actions, 0, wxEXPAND);
    auto *editors = new wxSplitterWindow(workspace, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE);
    request_ = new wxTextCtrl(editors, wxID_ANY, "{}", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_DONTWRAP);
    response_ = new wxTextCtrl(editors, wxID_ANY, w("响应（本机可见；不会自动写入文件）"), wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
    auto font = wxFontInfo(12).Family(wxFONTFAMILY_TELETYPE); request_->SetFont(font); response_->SetFont(font);
    editors->SplitVertically(request_, response_, 350); editors->SetMinimumPaneSize(200); editors->SetSashGravity(.45);
    ws->Add(editors, 1, wxEXPAND | wxALL, 6); workspace->SetSizer(ws);
    outer->SplitVertically(nav, workspace, 250); outer->SetMinimumPaneSize(200); root->Add(outer, 1, wxEXPAND | wxALL, 5);

    auto *foot = new wxPanel(this); auto *fsz = new wxBoxSizer(wxHORIZONTAL);
    fsz->Add(new wxStaticText(foot, wxID_ANY, w("事件自动轮询 · 只在内存保留最近记录")), 1, wxALIGN_CENTER_VERTICAL | wxALL, 8);
    button(foot, fsz, "清空显示", [this] { events_->Clear(); history_->Clear(); logs_->Clear(); response_->Clear(); diagnostics_.clear(); });
    button(foot, fsz, "导出诊断元数据…", [this] { export_report(); });
    foot->SetSizer(fsz); root->Add(foot, 0, wxEXPAND);
    auto *tabs = new wxNotebook(this, wxID_ANY);
    events_ = new wxTextCtrl(tabs, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
    history_ = new wxTextCtrl(tabs, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
    logs_ = new wxTextCtrl(tabs, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
    tabs->AddPage(events_, w("SDK 事件")); tabs->AddPage(history_, w("请求记录 / 耗时"));
    tabs->AddPage(logs_, w("SDK 本机日志"));
    tabs->SetMinSize(wxSize(-1, 150)); root->Add(tabs, 0, wxEXPAND | wxALL, 5);
    SetSizer(root); Centre(); populate(""); select("info");
    CallAfter([editors] { editors->SetSashPosition(editors->GetClientSize().GetWidth() / 2); });
    Bind(EVT_RESULT, &Frame::result, this);
    tree_->Bind(wxEVT_TREE_SEL_CHANGED, [this](wxTreeEvent &e) { auto *item = dynamic_cast<Item *>(tree_->GetItemData(e.GetItem())); if (item) select(item->op); });
    search->Bind(wxEVT_TEXT, [this, search](wxCommandEvent &) { populate(u(search->GetValue())); });
    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent &e) {
      if (!e.CanVeto()) { worker_.reset(); Destroy(); return; }
      if (closing_) { e.Veto(); return; }
      closing_ = true; e.Veto(); enqueue("stop"); SetStatusText(w("正在停止 SDK、提交数据…"));
    });
    worker_ = std::make_unique<Worker>(this);
    const auto exe = path_from_utf8(u(wxStandardPaths::Get().GetExecutablePath()));
#ifdef _WIN32
    auto bundled = exe.parent_path() / "libsovkit.dll";
#elif defined(__APPLE__)
    auto bundled = exe.parent_path().parent_path() / "Frameworks/libsovkit.dylib";
#else
    auto bundled = exe.parent_path() / "libsovkit.so";
#endif
    if (fs::exists(bundled)) library_->SetValue(w(utf8(bundled)));
    SetStatusText(w("未加载 SDK · 所有网络操作由你手动触发"));
    if (!smoke_.empty()) CallAfter([this] { enqueue("load", {{"path", u(library_->GetValue())}}); });
  }
private:
  template<class F> wxButton *button(wxWindow *p, wxSizer *s, const char *label, F callback) {
    auto *b = new wxButton(p, wxID_ANY, w(label)); s->Add(b, 0, wxALL | wxALIGN_CENTER_VERTICAL, 4);
    b->Bind(wxEVT_BUTTON, [callback](wxCommandEvent &) { callback(); }); return b;
  }
  void populate(const std::string &filter) {
    tree_->DeleteAllItems(); auto root = tree_->AddRoot("API"); std::map<std::string, wxTreeItemId> groups;
    std::vector<std::string> names{"selftest"}; for (auto x : operations()) names.push_back(x.name);
    for (const auto &op : names) {
      auto r = recipe(op); if (!filter.empty() && op.find(filter) == std::string::npos && r.title.find(filter) == std::string::npos && r.group.find(filter) == std::string::npos) continue;
      if (!groups.contains(r.group)) groups[r.group] = tree_->AppendItem(root, w(r.group));
      tree_->AppendItem(groups[r.group], w(r.title), -1, -1, new Item(op));
    }
    tree_->ExpandAll();
  }
  void select(const std::string &op) {
    selected_ = op; auto r = recipe(op); title_->SetLabel(w(r.title + "  /  " + op)); help_->SetValue(w(r.help)); request_->SetValue(w(r.request.dump(2)));
  }
  void enqueue(std::string op, Json request = Json::object()) {
    if (!worker_->submit({std::move(op), std::move(request)})) { closing_ = false; SetStatusText(w("请求队列已满，请等待当前操作完成")); return; }
    execute_->Disable(); start_->Disable(); load_->Disable(); ++pending_;
  }
  void execute() {
    try {
      auto r = recipe(selected_); auto data = Json::parse(u(request_->GetValue()));
      if (r.confirm && wxMessageBox(w("将执行 " + selected_ + "。请核对请求、授权和目标。"), w("确认手动操作"), wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) != wxYES) return;
      enqueue(selected_, data);
    } catch (...) { wxMessageBox(w("JSON 无效，未调用 SDK。"), w("请求格式"), wxOK | wxICON_ERROR, this); }
  }
  void append(wxTextCtrl *view, const std::string &text) {
    if (view->GetLastPosition() > 200000) view->Remove(0, view->GetLastPosition() - 100000);
    view->AppendText(w(text + "\n"));
  }
  void result(wxThreadEvent &event) {
    auto row = Json::parse(u(event.GetString())); const auto op = row.value("operation", "");
    if (op == "events") { append(events_, row.dump(2)); return; }
    if (op == "logs") { append(logs_, row["data"].dump(2)); return; }
    if (pending_) --pending_;
    if (op == "load" && row.value("code", -1) == 0) loaded_ = true;
    execute_->Enable(!closing_ && pending_ == 0); start_->Enable(!closing_ && pending_ == 0); load_->Enable(!loaded_ && !closing_ && pending_ == 0);
    response_->SetValue(w(row.dump(2)));
    append(history_, op + "  code=" + std::to_string(row.value("code", -1)) + "  " + std::to_string(row.value("elapsedMs", 0)) + " ms");
    diagnostics_.push_back(diagnostic(row)); if (diagnostics_.size() > 500) diagnostics_.pop_front();
    SetStatusText(w(op + (row.value("code", -1) == 0 ? " · 完成，请查看业务状态" : " · 失败，请查看响应")));
    if (!smoke_.empty() && op == "load" && row.value("code", -1) == 0) {
      SetStatusText(w("SDK " + row["data"].value("sdkVersion", "unknown")), 1);
      select("selftest");
      enqueue("selftest");
    } else if (!smoke_.empty() && (op == "selftest" || op == "load")) {
      Json report = diagnostic(row);
      report["clientWidth"] = GetClientSize().GetWidth(); report["clientHeight"] = GetClientSize().GetHeight();
      report["requestWidth"] = request_->GetSize().GetWidth(); report["responseWidth"] = response_->GetSize().GetWidth();
      std::ofstream output(smoke_, std::ios::binary); output << report.dump(2) << '\n';
      // The smoke runner closes this window after inspecting the rendered UI.
    }
    if (closing_ && op == "stop") {
      if (row.value("code", -1) == 0) { worker_.reset(); Destroy(); }
      else { closing_ = false; execute_->Enable(); start_->Enable(); wxMessageBox(w("SDK 尚未安全停止。请保留窗口，检查响应后重试停止。"), w("保留测试数据"), wxOK | wxICON_ERROR, this); }
    }
  }
  void export_report() {
    wxFileDialog dlg(this, w("导出操作名、状态码、耗时；不含请求、消息、身份、路径或密码"), "", "sovkit-diagnostics.json", "JSON|*.json", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;
    std::ofstream out(path_from_utf8(u(dlg.GetPath())), std::ios::binary);
    Json report{{"format", 1}, {"toolVersion", "0.1.0"}, {"records", diagnostics_}};
    out << report.dump(2) << '\n'; out.close();
    SetStatusText(out ? w("诊断元数据已导出") : w("导出失败"));
  }
  void show_docs() {
    auto *dlg = new wxDialog(this, wxID_ANY, w("SDK 接入文档"), wxDefaultPosition, wxSize(900, 700), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
    auto *text = new wxTextCtrl(dlg, wxID_ANY, w(kSdkDocs), wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
    auto *s = new wxBoxSizer(wxVERTICAL); s->Add(text, 1, wxEXPAND | wxALL, 10); dlg->SetSizer(s); dlg->ShowModal(); dlg->Destroy();
  }
  std::unique_ptr<Worker> worker_;
  wxTextCtrl *library_, *device_, *profile_, *password_, *request_, *response_, *help_, *events_, *history_, *logs_;
  wxStaticText *title_;
  wxTreeCtrl *tree_;
  wxButton *load_, *start_, *stop_, *execute_;
  std::string selected_;
  bool closing_ = false, loaded_ = false;
  size_t pending_ = 0;
  std::deque<Json> diagnostics_;
  fs::path smoke_;
};
class App : public wxApp {
  bool OnInit() override {
    SetAppName("SovKit DevTools");
    try {
      fs::path smoke;
      for (int i = 1; i + 1 < argc; ++i) if (wxString(argv[i]) == "--smoke-report") smoke = path_from_utf8(u(wxString(argv[++i])));
      (new Frame(smoke))->Show(); return true;
    }
    catch (const std::exception &e) { wxMessageBox(w(e.what()), "SovKit DevTools", wxOK | wxICON_ERROR); return false; }
  }
};
}
wxIMPLEMENT_APP(devtools::App);
