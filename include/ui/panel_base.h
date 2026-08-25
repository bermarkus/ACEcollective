#pragma once

#include <string>
#include <memory>
#include "imgui/imgui.h"
#include "ui/application_interface.h"

namespace ace {
namespace ui {

class PanelManager;

/**
 * Base class for all UI panels in the application. Ported from ACEapp's
 * ui::PanelBase.
 */
class PanelBase {
    friend class PanelManager;

public:
    PanelBase(ApplicationInterface* app, const std::string& title);
    virtual ~PanelBase() = default;

    bool is_visible() const { return visible_; }
    void set_visible(bool visible) { visible_ = visible; }
    const std::string& get_title() const { return title_; }
    ApplicationInterface* get_app() const { return app_; }

protected:
    // Must be implemented by derived panel classes.
    virtual void render_content() = 0;

    ApplicationInterface* app_;
    std::string title_;
    bool visible_;
    ImVec2 default_size_ = ImVec2(300, 300);
};

} // namespace ui
} // namespace ace
