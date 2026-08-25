#include "ui/panel_base.h"

namespace ace {
namespace ui {

PanelBase::PanelBase(ApplicationInterface* app, const std::string& title)
    : app_(app)
    , title_(title)
    , visible_(true)
{
}

} // namespace ui
} // namespace ace
