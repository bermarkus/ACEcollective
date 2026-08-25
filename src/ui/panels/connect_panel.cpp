#include "ui/panels/connect_panel.h"
#include "ui/theme.h"
#include "imgui/imgui.h"

namespace ace {
namespace ui {

ConnectPanel::ConnectPanel(ApplicationInterface* app) : PanelBase(app, "Connect") {}

void ConnectPanel::render_content()
{
    namespace T = ace::ui::theme;

    T::section_header("Discover Streams",
        "Lists ACEapp instances currently broadcasting an EEG stream on this network. "
        "Each participant should have ACEapp running with LSL streaming enabled.");

    if (ImGui::Button("Refresh")) {
        app_->refresh_discovered_streams();
    }
    ImGui::SameLine();
    T::text_status(T::StatusLevel::Muted, "%zu found", app_->get_discovered_stream_names().size());

    const auto& participants = app_->get_participants();
    for (const auto& name : app_->get_discovered_stream_names()) {
        bool already_added = false;
        for (const auto& p : participants) {
            if (p.lsl_stream_name == name) { already_added = true; break; }
        }
        ImGui::PushID(name.c_str());
        ImGui::TextUnformatted(name.c_str());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x > 120.0f
                            ? ImGui::GetWindowContentRegionMax().x - 80.0f : 0.0f);
        ImGui::BeginDisabled(already_added || participants.size() >= 6);
        if (ImGui::Button(already_added ? "Added" : "Add", ImVec2(70, 0))) {
            app_->add_participant(name);
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }

    T::section_header("Session Participants",
        "2-6 participants form one group session. Every pair's full "
        "cross-channel matrix is computed once the session is started.");

    for (const auto& p : participants) {
        ImGui::PushID(p.lsl_stream_name.c_str());

        // Participant identity color swatch, matching the color this
        // participant's channels will use in the connectivity view.
        static int color_cursor = 0;
        (void)color_cursor;
        ImVec4 swatch = T::kParticipantColors[
            static_cast<size_t>(&p - &participants[0]) % 6];
        ImGui::ColorButton("##color", swatch, ImGuiColorEditFlags_NoTooltip, ImVec2(14, 14));
        ImGui::SameLine();

        auto& buf = label_edit_buffers_[p.lsl_stream_name];
        if (buf.empty()) buf = p.participant_label;
        char text_buf[128];
        std::snprintf(text_buf, sizeof(text_buf), "%s", buf.c_str());
        ImGui::SetNextItemWidth(140);
        if (ImGui::InputText("##label", text_buf, sizeof(text_buf))) {
            buf = text_buf;
            app_->set_participant_label(p.lsl_stream_name, buf);
        }

        ImGui::SameLine();
        T::text_status(p.connected ? T::StatusLevel::Good : T::StatusLevel::Warn,
                       p.connected ? "connected" : "waiting");
        ImGui::SameLine();
        ImGui::Text("(%zu ch, %.0f Hz)", p.channel_names.size(), p.sample_rate);

        ImGui::SameLine();
        if (ImGui::Button("Remove")) {
            app_->remove_participant(p.lsl_stream_name);
            ImGui::PopID();
            break; // participants_ vector was mutated -- stop iterating this frame
        }

        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (app_->is_session_active()) {
        ImGui::PushStyleColor(ImGuiCol_Button, T::kActionRunning);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, T::kActionRunningHovered);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, T::kActionRunningActive);
        if (ImGui::Button("Stop Session", ImVec2(T::equal_button_width(1), 0))) {
            app_->stop_session();
        }
        ImGui::PopStyleColor(3);
    } else {
        ImGui::BeginDisabled(participants.size() < 2);
        if (ImGui::Button("Start Session", ImVec2(T::equal_button_width(1), 0))) {
            app_->start_session();
        }
        ImGui::EndDisabled();
        if (participants.size() < 2) {
            T::text_status(T::StatusLevel::Muted, "Add at least 2 participants to start");
        }
    }
}

} // namespace ui
} // namespace ace
