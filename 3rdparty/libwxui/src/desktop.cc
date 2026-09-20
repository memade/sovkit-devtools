#include <libwxui.hpp>
#include <libwxui/appearance.hpp>
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stdpaths.h>
#include <wx/statusbr.h>
#include <wx/weakref.h>
#include <mutex>

namespace wxui {

struct DesktopWindow::Impl {
    struct Dispatch {
        std::mutex mutex;
        wxEvtHandler* target = nullptr;
    };
    wxEvtHandler events;
    std::shared_ptr<Dispatch> dispatch = std::make_shared<Dispatch>();
    wxWeakRef<wxFrame> frame;
    UIManager* manager = nullptr;
    std::function<bool(bool)> onClose;
    bool closing = false;

    Impl() { dispatch->target = &events; }
    void Cancel() {
        { std::lock_guard lock(dispatch->mutex); dispatch->target = nullptr; }
        if (wxTheApp) wxTheApp->RemovePendingEventHandler(&events);
        events.DeletePendingEvents();
    }
    ~Impl() {
        Cancel();
        // The native handler captures this Impl; remove it before deferred destruction.
        if (frame) {
            frame->Unbind(wxEVT_CLOSE_WINDOW, &Impl::Close, this);
            frame->Destroy();
        }
    }
    void Close(wxCloseEvent& event) {
        if (closing) { if (event.CanVeto()) event.Veto(); return; }
        if (onClose && !onClose(event.CanVeto()) && event.CanVeto()) {
            event.Veto();
            return;
        }
        closing = true;
        Cancel();
        frame->Destroy();
    }
};

DesktopWindow::DesktopWindow(const DesktopWindowSpec& spec, const std::string& xml)
    : impl_(std::make_unique<Impl>()) {
    FrameSpec native;
    native.title = spec.title;
    native.size = {spec.size.width, spec.size.height};
    native.statusBar.enabled = spec.statusFields > 0;
    native.statusBar.fields = spec.statusFields;
    auto* frame = new SdiFrame(native);
    frame->SetFont(InterfaceFont());
    if (frame->GetStatusBar()) frame->GetStatusBar()->SetFont(InterfaceFont());
    impl_->frame = frame;
    frame->SetMinSize({spec.minimumSize.width, spec.minimumSize.height});
    if (!frame->LoadContentXml(xml)) throw std::runtime_error("Invalid window UI XML");
    impl_->manager = frame->GetUIManager();
    frame->Bind(wxEVT_CLOSE_WINDOW, &Impl::Close, impl_.get());
}

DesktopWindow::~DesktopWindow() = default;
Control* DesktopWindow::FindControl(const std::string& name) const {
    return impl_->frame && !impl_->closing ? impl_->manager->FindControl(name) : nullptr;
}
void DesktopWindow::Present() { if (impl_->frame) impl_->frame->Show(); }
void DesktopWindow::SetStatus(const std::string& text, int field) {
    if (impl_->frame) SetFrameStatusText(impl_->frame, Utf8ToWxString(text), field);
}
Extent DesktopWindow::ClientExtent() const {
    if (!impl_->frame) return {};
    const auto size = impl_->frame->GetClientSize();
    return {size.x, size.y};
}
void DesktopWindow::RefreshLayout() {
    if (impl_->frame && !impl_->closing) impl_->manager->ForceLayout();
}
UiPost DesktopWindow::Poster() const {
    return [state = impl_->dispatch](std::function<void()> callback) {
        std::lock_guard lock(state->mutex);
        if (!state->target) return false;
        state->target->CallAfter(std::move(callback));
        return true;
    };
}
void DesktopWindow::OnClose(std::function<bool(bool)> callback) { impl_->onClose = std::move(callback); }
void DesktopWindow::RequestClose() { if (impl_->frame) impl_->frame->Close(); }
void DesktopWindow::FinishClose() {
    impl_->closing = true;
    impl_->Cancel();
    if (impl_->frame) impl_->frame->Destroy();
}
std::optional<std::string> DesktopWindow::OpenFile(const std::string& title, const std::string& filter) {
    wxFileDialog dialog(impl_->frame, Utf8ToWxString(title), {}, {}, Utf8ToWxString(filter),
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) return std::nullopt;
    return WxStringToUtf8(dialog.GetPath());
}
std::optional<std::string> DesktopWindow::SaveFile(const std::string& title,
                                                 const std::string& filename, const std::string& filter) {
    wxFileDialog dialog(impl_->frame, Utf8ToWxString(title), {}, Utf8ToWxString(filename),
                        Utf8ToWxString(filter), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return std::nullopt;
    return WxStringToUtf8(dialog.GetPath());
}
std::optional<std::string> DesktopWindow::ChooseDirectory(const std::string& title) {
    wxDirDialog dialog(impl_->frame, Utf8ToWxString(title));
    if (dialog.ShowModal() != wxID_OK) return std::nullopt;
    return WxStringToUtf8(dialog.GetPath());
}
bool DesktopWindow::Confirm(const std::string& title, const std::string& message) {
    return wxMessageBox(Utf8ToWxString(message), Utf8ToWxString(title),
                        wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, impl_->frame) == wxYES;
}
void DesktopWindow::Error(const std::string& title, const std::string& message) {
    wxMessageBox(Utf8ToWxString(message), Utf8ToWxString(title), wxOK | wxICON_ERROR, impl_->frame);
}
void DesktopWindow::ShowText(const std::string& title, std::string_view text) {
    wxDialog dialog(impl_->frame, wxID_ANY, Utf8ToWxString(title), wxDefaultPosition,
                    {900, 700}, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
    dialog.SetFont(InterfaceFont());
    auto* manager = new UIManager(&dialog);
    if (!manager->LoadFromString(std::string(R"(<Window><VerticalLayout inset="10,10,10,10"><RichEdit name="text" readonly="true" wanttab="false"/></VerticalLayout></Window>)")))
        throw std::runtime_error("Invalid text dialog XML");
    auto* edit = dynamic_cast<RichEdit*>(manager->FindControl("text"));
    edit->SetValue(text);
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(manager, 1, wxEXPAND);
    dialog.SetSizer(sizer);
    dialog.CentreOnParent();
    dialog.ShowModal();
}
std::string ExecutablePath() { return WxStringToUtf8(wxStandardPaths::Get().GetExecutablePath()); }
std::string HostName() { return WxStringToUtf8(wxGetHostName()); }
void ShowError(const std::string& title, const std::string& message) {
    wxMessageBox(Utf8ToWxString(message), Utf8ToWxString(title), wxOK | wxICON_ERROR);
}

} // namespace wxui
