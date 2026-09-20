#pragma once
#include <wx/dc.h>
#include <wx/font.h>

namespace wxui {
// System-installed fonts only: Microsoft YaHei on Windows, native GUI and
// monospace defaults on macOS/Linux. No font files or private registration.
void InitializeTypography();
wxFont InterfaceFont();
wxFont CodeFont();
struct ChromeColors {
    wxColour track, thumb, active, divider;
};
ChromeColors ChromeFor(const wxColour& background);
void PaintScrollChrome(wxDC& dc, const wxRect& track, const wxRect& thumb,
                       bool active, const wxColour& background);
void PaintDivider(wxDC& dc, const wxRect& rect, bool vertical, bool active,
                  const wxColour& background);
} // namespace wxui
