#pragma once

#include "ui/panel_base.h"
#include <string>
#include <unordered_map>

namespace ace {
namespace ui {

// Discovers ACEapp-* EEG streams on the network and lets the user pick 2-6
// of them to form the current group session (manual selection, per the
// architecture decision recorded for this project -- no naming-convention
// auto-grouping).
class ConnectPanel : public PanelBase {
public:
    explicit ConnectPanel(ApplicationInterface* app);
    void render_content() override;

private:
    // Per-stream-name label edit buffers, so ImGui::InputText has stable
    // backing storage independent of ParticipantStreamInfo's own string.
    std::unordered_map<std::string, std::string> label_edit_buffers_;
};

} // namespace ui
} // namespace ace
