#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

#include "ui/panels/connect_panel.h"
#include "ui/panels/interbrain_connectivity_panel.h"
#include "ui/panels/settings_panel.h"
#include "ui/application_interface.h"

namespace ace {
namespace ui {

/**
 * @brief Manages ACEcollective's UI panels.
 *
 * A deliberately simpler sibling of ACEapp's PanelManager (ui/panel_manager.h
 * there): same visual language (push_panel_style()/pop_panel_style() pull
 * from the shared theme.h, same tab-bar look) but without ACEapp's
 * auto-fit-width/collapse-arrow mechanics, which exist there to host many
 * tabs across a long-running acquisition session. ACEcollective only has two
 * side-panel tabs (Connect, Settings); the InterBrainConnectivityPanel is
 * the main workspace content instead of a tab, the same role ACEapp's
 * SignalDisplay plays in its own main viewport.
 */
class PanelManager {
public:
    PanelManager(ApplicationInterface* app);
    ~PanelManager() = default;

    void initialize();

    // Renders the side control panel (Connect/Settings tabs) and the main
    // workspace (InterBrainConnectivityPanel) in one call.
    void render_panels(ImFont* large_font, ImFont* default_font);

    template<typename T>
    T* get_panel() {
        for (auto& panel : panels_) {
            T* typed_panel = dynamic_cast<T*>(panel.get());
            if (typed_panel) return typed_panel;
        }
        return nullptr;
    }

    PanelBase* get_panel_by_name(const std::string& name);

private:
    ApplicationInterface* app_;
    std::vector<std::unique_ptr<PanelBase>> panels_;
    std::unordered_map<std::string, PanelBase*> panels_by_name_;

    void push_panel_style();
    void pop_panel_style();
};

} // namespace ui
} // namespace ace
