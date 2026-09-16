#include <libwxui.hpp>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <cctype>
#include <cstddef>

namespace wxui {
namespace {

constexpr std::size_t kMaxHighlightedChars = 512 * 1024;

std::string JsonToText(const rapidjson::Value& value, bool pretty) {
    rapidjson::StringBuffer buffer;
    if (pretty) {
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        value.Accept(writer);
    } else {
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        value.Accept(writer);
    }
    return std::string(buffer.GetString(), buffer.GetSize());
}

std::string NormalizeJson(std::string_view raw, JsonViewMode mode) {
    if (mode == JsonViewMode::Raw || raw.empty()) {
        return std::string(raw);
    }

    rapidjson::Document doc;
    if (doc.Parse(raw.data(), raw.size()).HasParseError()) {
        return std::string(raw);
    }
    return JsonToText(doc, mode == JsonViewMode::Pretty);
}

JsonViewMode ParseJsonViewMode(const std::string& value, JsonViewMode fallback) {
    const std::string normalized = NormalizeXmlIdentifier(value);
    if (normalized == "raw") return JsonViewMode::Raw;
    if (normalized == "min" || normalized == "mini" ||
        normalized == "minify" || normalized == "minified" ||
        normalized == "compact") {
        return JsonViewMode::Minified;
    }
    if (normalized == "format" || normalized == "formatted" ||
        normalized == "pretty") {
        return JsonViewMode::Pretty;
    }
    return fallback;
}

wxColour ResolveJsonTextColor(UIManager* manager, const wxColour& requested) {
    if (requested.IsOk()) return requested;
    if (manager && manager->GetRoot()) {
        return manager->GetRoot()->GetDefaultFontColor();
    }
    return *wxWHITE;
}

wxFont ResolveJsonFont(UIManager* manager) {
    wxFont base = manager ? manager->GetUIFont() : wxFont{};
    wxFontInfo info(base.IsOk() && base.GetPointSize() > 0 ? base.GetPointSize() : 10);
    info.Family(wxFONTFAMILY_TELETYPE);
    return wxFont(info);
}

bool IsJsonWhitespace(wxChar ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

bool IsNumberStart(wxChar ch) {
    return ch == '-' || (ch >= '0' && ch <= '9');
}

bool IsNumberChar(wxChar ch) {
    return (ch >= '0' && ch <= '9') || ch == '-' || ch == '+' ||
           ch == '.' || ch == 'e' || ch == 'E';
}

bool StartsWithLiteral(const wxString& text, std::size_t pos,
                       const wxString& literal) {
    if (pos + literal.length() > text.length()) return false;
    return text.Mid(pos, literal.length()) == literal;
}

void ApplyRange(wxTextCtrl* ctrl, long start, long end, const wxColour& color) {
    if (!ctrl || end <= start) return;
    wxTextAttr attr;
    attr.SetTextColour(color);
    ctrl->SetStyle(start, end, attr);
}

} // namespace

JsonViewer::~JsonViewer() = default;

void JsonViewer::SetAttribute(const std::string& key, const std::string& val) {
    if (key == "text" || key == "json" || key == "rawjson" || key == "rawJson") {
        SetJson(val);
        return;
    }
    if (key == "mode" || key == "viewmode" || key == "viewMode") {
        SetMode(ParseJsonViewMode(val, mode_));
        return;
    }
    if (key == "wraplines" || key == "wrapLines" || key == "wrap") {
        SetWrapLines(ParseBOOL(val, true));
        return;
    }
    if (key == "transparent") {
        transparent_ = ParseBOOL(val, true);
        return;
    }
    if (key == "textcolor") {
        textColor_ = ParseDWORD(val);
        SyncStyle();
        return;
    }
    if (key == "font") {
        fontId_ = ParseINT(val, -1);
        SyncStyle();
        return;
    }
    Container::SetAttribute(key, val);
}

void JsonViewer::OnManagerSet() {
    CreateNativeCtrls();
}

void JsonViewer::CreateNativeCtrls() {
    if (!manager_ || wrappedCtrl_) return;

    const long baseStyle = wxBORDER_NONE | wxTE_MULTILINE | wxTE_READONLY |
                           wxTE_RICH2 | wxTE_PROCESS_TAB | wxVSCROLL;
    wrappedCtrl_ = new wxTextCtrl(manager_, wxID_ANY, wxString{},
                                  rect_.GetTopLeft(), rect_.GetSize(),
                                  baseStyle);
    nowrapCtrl_ = new wxTextCtrl(manager_, wxID_ANY, wxString{},
                                 rect_.GetTopLeft(), rect_.GetSize(),
                                 baseStyle | wxTE_DONTWRAP | wxHSCROLL);
    SyncStyle();
    RefreshText();
}

void JsonViewer::SyncStyle() {
    for (wxTextCtrl* ctrl : {wrappedCtrl_, nowrapCtrl_}) {
        if (!ctrl) continue;
        ctrl->SetFont(ResolveJsonFont(manager_));
        ctrl->SetOwnForegroundColour(ResolveJsonTextColor(manager_, textColor_));
        if (bkColor_.IsOk() && bkColor_.Alpha() != 0) {
            ctrl->SetOwnBackgroundColour(bkColor_);
        }
        ctrl->SetEditable(false);
        ctrl->Enable(enabled_);
        ctrl->Show(IsNativeWindowVisible() &&
                   ((ctrl == wrappedCtrl_) == wrapLines_));
    }
}

void JsonViewer::SetRect(const wxRect& rc) {
    Control::SetRect(rc);
    const int inset = std::max(0, borderSize_);
    const wxRect inner(rc.x + inset, rc.y + inset,
                       std::max(0, rc.width - inset * 2),
                       std::max(0, rc.height - inset * 2));
    for (wxTextCtrl* ctrl : {wrappedCtrl_, nowrapCtrl_}) {
        if (!ctrl) continue;
        ctrl->SetPosition(inner.GetTopLeft());
        ctrl->SetSize(inner.GetSize());
        ctrl->Show(IsNativeWindowVisible() &&
                   ((ctrl == wrappedCtrl_) == wrapLines_));
    }
}

void JsonViewer::SetVisible(bool v) {
    Control::SetVisible(v);
    SyncStyle();
}

void JsonViewer::SetEnabled(bool e) {
    Control::SetEnabled(e);
    SyncStyle();
}

void JsonViewer::SetJson(std::string_view json) {
    rawJson_ = std::string(json);
    prettyJson_.clear();
    minifiedJson_.clear();
    prettyJsonValid_ = false;
    minifiedJsonValid_ = false;
    RefreshText();
}

void JsonViewer::SetMode(JsonViewMode mode) {
    if (mode_ == mode) return;
    mode_ = mode;
    RefreshText();
}

void JsonViewer::SetWrapLines(bool wrap) {
    if (wrapLines_ == wrap) return;
    wrapLines_ = wrap;
    SyncStyle();
    RenderActiveText();
    if (auto* ctrl = ActiveTextCtrl()) {
        ctrl->SetFocus();
    }
}

std::string JsonViewer::GetDisplayedText() const {
    return displayedText_;
}

wxTextCtrl* JsonViewer::ActiveTextCtrl() const {
    return wrapLines_ ? wrappedCtrl_ : nowrapCtrl_;
}

bool& JsonViewer::ActiveRenderedFlag() {
    return wrapLines_ ? wrappedRendered_ : nowrapRendered_;
}

void JsonViewer::InvalidateRenderedControls() {
    wrappedRendered_ = false;
    nowrapRendered_ = false;
}

const std::string& JsonViewer::TextForMode(JsonViewMode mode) {
    if (mode == JsonViewMode::Raw) return rawJson_;
    if (mode == JsonViewMode::Pretty) {
        if (!prettyJsonValid_) {
            prettyJson_ = NormalizeJson(rawJson_, JsonViewMode::Pretty);
            prettyJsonValid_ = true;
        }
        return prettyJson_;
    }
    if (!minifiedJsonValid_) {
        minifiedJson_ = NormalizeJson(rawJson_, JsonViewMode::Minified);
        minifiedJsonValid_ = true;
    }
    return minifiedJson_;
}

void JsonViewer::RefreshText() {
    displayedText_ = TextForMode(mode_);
    InvalidateRenderedControls();
    RenderActiveText();
}

void JsonViewer::RenderActiveText() {
    wxTextCtrl* ctrl = ActiveTextCtrl();
    if (!ctrl) return;
    bool& rendered = ActiveRenderedFlag();
    if (rendered) return;
    SetTextCtrlValue(ctrl, Utf8ToWxString(displayedText_));
    rendered = true;
}

void JsonViewer::SetTextCtrlValue(wxTextCtrl* ctrl, const wxString& text) {
    if (!ctrl) return;
    ctrl->Freeze();
    ctrl->SetEditable(true);
    ctrl->SetValue(text);
    ctrl->SetInsertionPoint(0);
    ctrl->ShowPosition(0);
    ctrl->SetEditable(false);
    ApplyJsonHighlight(ctrl, text);
    ctrl->Thaw();
}

void JsonViewer::ApplyJsonHighlight(wxTextCtrl* ctrl, const wxString& text) {
    if (!ctrl) return;

    const long length = static_cast<long>(text.length());
    if (length <= 0) return;
    if (text.length() > kMaxHighlightedChars) {
        return;
    }

    wxTextAttr base;
    base.SetTextColour(ResolveJsonTextColor(manager_, textColor_));
    if (bkColor_.IsOk()) base.SetBackgroundColour(bkColor_);
    ctrl->SetStyle(0, length, base);

    const wxColour keyColor(0x9C, 0xDC, 0xFE);
    const wxColour stringColor(0xCE, 0x91, 0x78);
    const wxColour numberColor(0xB5, 0xCE, 0xA8);
    const wxColour literalColor(0x56, 0x9C, 0xD6);

    std::size_t i = 0;
    while (i < text.length()) {
        const wxChar ch = text[i];
        if (ch == '"') {
            const std::size_t start = i;
            ++i;
            bool escaped = false;
            while (i < text.length()) {
                const wxChar current = text[i++];
                if (escaped) {
                    escaped = false;
                    continue;
                }
                if (current == '\\') {
                    escaped = true;
                    continue;
                }
                if (current == '"') break;
            }
            const std::size_t end = i;
            std::size_t lookahead = end;
            while (lookahead < text.length() && IsJsonWhitespace(text[lookahead])) {
                ++lookahead;
            }
            const bool isKey = lookahead < text.length() && text[lookahead] == ':';
            ApplyRange(ctrl, static_cast<long>(start), static_cast<long>(end),
                       isKey ? keyColor : stringColor);
            continue;
        }

        if (IsNumberStart(ch)) {
            const std::size_t start = i;
            ++i;
            while (i < text.length() && IsNumberChar(text[i])) ++i;
            ApplyRange(ctrl, static_cast<long>(start), static_cast<long>(i),
                       numberColor);
            continue;
        }

        if (StartsWithLiteral(text, i, "true")) {
            ApplyRange(ctrl, static_cast<long>(i), static_cast<long>(i + 4),
                       literalColor);
            i += 4;
            continue;
        }
        if (StartsWithLiteral(text, i, "false")) {
            ApplyRange(ctrl, static_cast<long>(i), static_cast<long>(i + 5),
                       literalColor);
            i += 5;
            continue;
        }
        if (StartsWithLiteral(text, i, "null")) {
            ApplyRange(ctrl, static_cast<long>(i), static_cast<long>(i + 4),
                       literalColor);
            i += 4;
            continue;
        }

        ++i;
    }
}

void JsonViewer::DoPaint(wxDC& dc, const wxRect& clipRect) {
    if (!transparent_) Control::DoPaint(dc, clipRect);
    DrawBorder(dc);
}

} // namespace wxui
