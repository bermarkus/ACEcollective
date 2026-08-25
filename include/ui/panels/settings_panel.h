#pragma once

#include "ui/panel_base.h"
#include <string>

namespace ace {
namespace ui {

// Connectivity engine configuration (metric, frequency band, window size)
// and recording/republishing controls.
class SettingsPanel : public PanelBase {
public:
    explicit SettingsPanel(ApplicationInterface* app);
    void render_content() override;

private:
    char recording_filename_[64] = "session";
    // Custom band editing buffers, applied via set_freq_band() on edit.
    float custom_fmin_ = 8.0f;
    float custom_fmax_ = 12.0f;
};

} // namespace ui
} // namespace ace
