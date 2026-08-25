#include "ui/panel_manager.h"
#include "ui/theme.h"

namespace ace {
namespace ui {

PanelManager::PanelManager(ApplicationInterface* app) : app_(app) {}

void PanelManager::initialize()
{
    auto connect_panel = std::make_unique<ConnectPanel>(app_);
    auto connectivity_panel = std::make_unique<InterBrainConnectivityPanel>(app_);
    auto settings_panel = std::make_unique<SettingsPanel>(app_);

    panels_by_name_[connect_panel->get_title()] = connect_panel.get();
    panels_by_name_[connectivity_panel->get_title()] = connectivity_panel.get();
    panels_by_name_[settings_panel->get_title()] = settings_panel.get();

    panels_.push_back(std::move(connect_panel));
    panels_.push_back(std::move(connectivity_panel));
    panels_.push_back(std::move(settings_panel));
}

PanelBase* PanelManager::get_panel_by_name(const std::string& name)
{
    auto it = panels_by_name_.find(name);
    return it != panels_by_name_.end() ? it->second : nullptr;
}

void PanelManager::push_panel_style()
{
    namespace T = ace::ui::theme;
    ImGui::PushStyleColor(ImGuiCol_WindowBg,       T::kBgPanel);
    ImGui::PushStyleColor(ImGuiCol_Border,         T::kBorderSubtle);
    ImGui::PushStyleColor(ImGuiCol_TitleBg,        T::kBgPanelTitle);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,  T::kBgPanelTitleAct);
    ImGui::PushStyleColor(ImGuiCol_Header,         T::kControlBg);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,  T::kHoverAccent);
    ImGui::PushStyleColor(ImGuiCol_Button,         T::kButton);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  T::kButtonHovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   T::kButtonActive);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        T::kFrameBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, T::kFrameBgHovered);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  T::kFrameBgActive);
    ImGui::PushStyleColor(ImGuiCol_Tab,            T::kTab);
    ImGui::PushStyleColor(ImGuiCol_TabHovered,     T::kTabHovered);
    ImGui::PushStyleColor(ImGuiCol_TabActive,      T::kTabActive);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, T::kRoundingPanel);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,  T::kRoundingFrame);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding,  T::kRoundingPopup);
    ImGui::PushStyleVar(ImGuiStyleVar_TabRounding,    T::kRoundingTab);
}

void PanelManager::pop_panel_style()
{
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(15); // must match the push count in push_panel_style() above
}

void PanelManager::render_panels(ImFont* /*large_font*/, ImFont* /*default_font*/)
{
    namespace T = ace::ui::theme;

    // Main workspace: teal background (matches ACEapp's signal-display
    // workspace) hosting the InterBrainConnectivityPanel full-time -- this
    // is ACEcollective's primary content, the same role SignalDisplay plays
    // in ACEapp's main viewport.
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, T::kBgWorkspace);
    ImGuiWindowFlags workspace_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    if (ImGui::Begin("##Workspace", nullptr, workspace_flags)) {
        // Leave room on the left for the floating control panel.
        ImGui::Dummy(ImVec2(340, 0));
        ImGui::SameLine();
        ImGui::BeginGroup();
        auto* connectivity = get_panel<InterBrainConnectivityPanel>();
        if (connectivity) connectivity->render_content();
        ImGui::EndGroup();
    }
    ImGui::End();
    ImGui::PopStyleColor();

    // Side control panel: Connect / Settings tabs, same visual language as
    // ACEapp's control panel (dark background, rounded, accent-blue tabs).
    push_panel_style();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + 16, viewport->Pos.y + 16), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, viewport->Size.y - 32), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("ACEcollective", nullptr, ImGuiWindowFlags_NoCollapse)) {
        // One scrollable column, not a tab bar -- connection/participant
        // management on top since that's what you touch first and most
        // often, engine/recording settings below it.
        if (auto* connect_panel = get_panel_by_name("Connect")) {
            ImGui::PushID("Connect");
            connect_panel->render_content();
            ImGui::PopID();
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (auto* settings_panel = get_panel_by_name("Settings")) {
            ImGui::PushID("Settings");
            settings_panel->render_content();
            ImGui::PopID();
        }
    }
    ImGui::End();
    pop_panel_style();
}

} // namespace ui
} // namespace ace
