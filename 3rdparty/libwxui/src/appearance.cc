#include <libwxui/appearance.hpp>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/fontenum.h>
#include <mutex>
#include <algorithm>
#ifdef __WXMSW__
#include <wx/msw/wrapwin.h>
#elif defined(__WXOSX__)
#include <CoreText/CoreText.h>
#else
#include <fontconfig/fontconfig.h>
#include <pango/pangocairo.h>
#include <pango/pangofc-fontmap.h>
#endif

namespace wxui {
namespace {
wxFont uiFont, codeFont;
std::once_flag initialized;
bool RegisterFont(const wxString& path) {
#ifdef __WXMSW__
    return AddFontResourceExW(path.wc_str(), FR_PRIVATE, nullptr) > 0;
#elif defined(__WXOSX__)
    const auto bytes = path.ToUTF8();
    auto url = CFURLCreateFromFileSystemRepresentation(nullptr,
        reinterpret_cast<const UInt8*>(bytes.data()), bytes.length(), false);
    if (!url) return false;
    const bool result = CTFontManagerRegisterFontsForURL(url, kCTFontManagerScopeProcess, nullptr);
    CFRelease(url);
    return result;
#else
    const auto bytes = path.ToUTF8();
    if (!FcConfigAppFontAddFile(FcConfigGetCurrent(), reinterpret_cast<const FcChar8*>(bytes.data()))) return false;
    auto* map = pango_cairo_font_map_get_default();
    if (PANGO_IS_FC_FONT_MAP(map)) pango_fc_font_map_config_changed(PANGO_FC_FONT_MAP(map));
    return true;
#endif
}
}
void InitializeTypography() {
    std::call_once(initialized, [] {
        wxFileName executable(wxStandardPaths::Get().GetExecutablePath());
        wxString directory = executable.GetPath() + "/fonts/";
#ifdef __WXOSX__
        const auto bundled = executable.GetPath() + "/../Resources/fonts/";
        if (wxDirExists(bundled)) directory = bundled;
#endif
        for (const auto* filename : {"NotoSansCJKsc-Regular.otf", "NotoSansMonoCJKsc-Regular.otf"}) {
            const wxString path = directory + filename;
            if (wxFileExists(path)) RegisterFont(path);
        }
        wxFontEnumerator::InvalidateCache();
        wxFontInfo ui(11), code(11);
        ui.Family(wxFONTFAMILY_SWISS);
        code.Family(wxFONTFAMILY_TELETYPE);
        if (wxFontEnumerator::IsValidFacename("Noto Sans CJK SC")) ui.FaceName("Noto Sans CJK SC");
        if (wxFontEnumerator::IsValidFacename("Noto Sans Mono CJK SC")) code.FaceName("Noto Sans Mono CJK SC");
        uiFont = wxFont(ui); codeFont = wxFont(code);
    });
}
wxFont InterfaceFont() { InitializeTypography(); return uiFont; }
wxFont CodeFont() { InitializeTypography(); return codeFont; }
ChromeColors ChromeFor(const wxColour& background) {
    const bool dark = background.IsOk() &&
        (background.Red() * 299 + background.Green() * 587 + background.Blue() * 114 < 128000);
    if (dark) return {{38, 45, 55}, {99, 113, 130}, {137, 165, 191}, {73, 87, 103}};
    return {{240, 244, 248}, {174, 187, 201}, {99, 134, 163}, {200, 211, 222}};
}
void PaintScrollChrome(wxDC& dc, const wxRect& track, const wxRect& thumb,
                       bool active, const wxColour& background) {
    const auto colors = ChromeFor(background);
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(colors.track));
    dc.DrawRectangle(track);
    if (thumb.IsEmpty()) return;
    dc.SetBrush(wxBrush(active ? colors.active : colors.thumb));
    dc.DrawRoundedRectangle(thumb, std::min(thumb.width, thumb.height) / 2.0);
}
void PaintDivider(wxDC& dc, const wxRect& rect, bool vertical, bool active,
                  const wxColour& background) {
    const auto colors = ChromeFor(background);
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(background.IsOk() ? background : colors.track));
    dc.DrawRectangle(rect);
    dc.SetPen(wxPen(active ? colors.active : colors.divider));
    const wxPoint center(rect.x + rect.width / 2, rect.y + rect.height / 2);
    if (vertical) dc.DrawLine(center.x, rect.y, center.x, rect.GetBottom() + 1);
    else dc.DrawLine(rect.x, center.y, rect.GetRight() + 1, center.y);
    dc.SetBrush(wxBrush(active ? colors.active : colors.thumb));
    dc.SetPen(*wxTRANSPARENT_PEN);
    if (vertical) dc.DrawRoundedRectangle(center.x - 1, center.y - 12, 3, 24, 1);
    else dc.DrawRoundedRectangle(center.x - 12, center.y - 1, 24, 3, 1);
}
} // namespace wxui
