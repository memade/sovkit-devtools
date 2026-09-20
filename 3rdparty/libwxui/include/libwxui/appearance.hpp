#pragma once
#include <wx/dc.h>
#include <wx/font.h>

namespace wxui {
// Fonts are registered privately for this process from the packaged fonts directory.
// Missing resources fall back to a platform sans-serif/monospace font.
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
