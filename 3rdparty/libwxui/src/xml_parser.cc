#include <libwxui.hpp>

#include <wx/xml/xml.h>
#include <wx/sstream.h>
#include <wx/log.h>
#include <stdexcept>

namespace wxui {

// ── Forward declaration ───────────────────────────────────────────────────
static std::shared_ptr<Control> BuildControlFromNode(wxXmlNode* node,
                                                      UIManager* mgr);

// ── Recursive builder ─────────────────────────────────────────────────────
static std::shared_ptr<Control> BuildControlFromNode(wxXmlNode* node,
                                                      UIManager* mgr) {
    if (!node || node->GetType() != wxXML_ELEMENT_NODE)
        return {};

    const std::string rawTag = WxStringToUtf8(node->GetName());
    auto ctrl = ControlFactory::Instance().Create(rawTag);
    if (!ctrl) {
        wxLogWarning("libwxui: unknown tag <%s> — skipped", rawTag.c_str());
        return {};
    }

    // Apply XML attributes
    wxXmlAttribute* attr = node->GetAttributes();
    while (attr) {
        ctrl->SetAttribute(NormalizeXmlIdentifier(WxStringToUtf8(attr->GetName())),
                           WxStringToUtf8(attr->GetValue()));
        attr = attr->GetNext();
    }

    // Propagate manager pointer after SetAttribute so native-backed controls
    // are created with the final XML style/text values already available.
    ctrl->SetManager(mgr);

    // Recurse into children (only meaningful for Container subclasses)
    auto* container = dynamic_cast<Container*>(ctrl.get());
    if (container) {
        for (wxXmlNode* child = node->GetChildren();
             child;
             child = child->GetNext()) {
            if (child->GetType() != wxXML_ELEMENT_NODE) continue;
            auto childCtrl = BuildControlFromNode(child, mgr);
            if (childCtrl) container->Add(childCtrl);
        }
    }

    return ctrl;
}

// ── UIManager — LoadFromFile ──────────────────────────────────────────────
bool UIManager::LoadFromFile(const std::string& xmlPath) {
    wxXmlDocument doc;
    if (!doc.Load(Utf8ToWxString(xmlPath))) {
        wxLogError("libwxui: failed to load XML from '%s'", xmlPath.c_str());
        return false;
    }
    return BuildFromXmlDoc(doc);
}

bool UIManager::LoadFromResource(const std::string& xmlPath) {
    std::string xml;
    if (LoadResourceBytes(xmlPath, &xml)) {
        return LoadFromString(xml);
    }

    const std::string full = resRoot_.empty() ? xmlPath : resRoot_ + "/" + xmlPath;
    return LoadFromFile(full);
}

// ── UIManager — LoadFromString ────────────────────────────────────────────
bool UIManager::LoadFromString(const std::string& xmlContent) {
    wxStringInputStream stream(Utf8ToWxString(xmlContent));
    wxXmlDocument doc;
    if (!doc.Load(stream)) {
        wxLogError("libwxui: failed to parse inline XML");
        return false;
    }
    return BuildFromXmlDoc(doc);
}

// ── UIManager — BuildFromXmlDoc ───────────────────────────────────────────
bool UIManager::BuildFromXmlDoc(wxXmlDocument& doc) {
    wxXmlNode* root = doc.GetRoot();
    if (!root) return false;

    auto ctrl = BuildControlFromNode(root, this);
    if (!ctrl) return false;

    auto* win = dynamic_cast<Window*>(ctrl.get());
    ForgetControlTree(root_.get());
    if (win) {
        root_ = std::static_pointer_cast<Window>(ctrl);
    } else {
        wxLogWarning("libwxui: root element <%s> is not <Window> — wrapping",
                     root->GetName().c_str());
        root_ = std::make_shared<Window>();
        root_->Add(ctrl);
    }

    root_->SetManager(this);
    const wxColour rootBk = root_->GetBkColor();
    if (rootBk.IsOk() && rootBk.Alpha() != 0) {
        SetBackgroundColour(rootBk);
    }
    DoLayout();
    return true;
}

} // namespace wxui
