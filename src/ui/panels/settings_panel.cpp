#include "ui/panels/settings_panel.h"
#include "ui/theme.h"
#include "processing/interbrain_connectivity_calculator.h"
#include "imgui/imgui.h"

namespace ace {
namespace ui {

SettingsPanel::SettingsPanel(ApplicationInterface* app) : PanelBase(app, "Settings")
{
    custom_fmin_ = app_->get_freq_min();
    custom_fmax_ = app_->get_freq_max();
}

void SettingsPanel::render_content()
{
    namespace T = ace::ui::theme;
    namespace P = ace::processing;

    T::section_header("View");
    ConnectivityViewType view = app_->get_view_type();
    const char* view_names[] = {
        "Pair Detail",
        "Block Matrix (all channels, all pairs)",
        "Circular (all channels, all pairs)",
        "Small Multiples (one mini heatmap per pair)",
        "Force-Directed (channels pulled by coherence)",
    };
    int view_index = 0;
    switch (view) {
        case ConnectivityViewType::PairDetail:     view_index = 0; break;
        case ConnectivityViewType::BlockMatrix:    view_index = 1; break;
        case ConnectivityViewType::Circular:       view_index = 2; break;
        case ConnectivityViewType::SmallMultiples: view_index = 3; break;
        case ConnectivityViewType::ForceDirected:  view_index = 4; break;
    }
    ImGui::SetNextItemWidth(300);
    if (ImGui::Combo("##ViewType", &view_index, view_names, IM_ARRAYSIZE(view_names))) {
        static const ConnectivityViewType kViews[] = {
            ConnectivityViewType::PairDetail, ConnectivityViewType::BlockMatrix,
            ConnectivityViewType::Circular, ConnectivityViewType::SmallMultiples,
            ConnectivityViewType::ForceDirected,
        };
        app_->set_view_type(kViews[view_index]);
    }

    T::section_header("Connection Threshold",
        "Minimum value a pair must reach to be drawn as a visible connection in "
        "Pair Detail, Circular, and Force-Directed. Doesn't affect Block Matrix or "
        "Small Multiples, which show every cell's actual value as a heatmap.");
    float threshold = app_->get_connection_threshold();
    ImGui::SetNextItemWidth(200);
    if (ImGui::SliderFloat("##ConnectionThreshold", &threshold, 0.0f, 1.0f, "%.2f")) {
        app_->set_connection_threshold(threshold);
    }

    T::section_header("Channel Pairing",
        "Full Matrix compares every channel of A against every channel of B. "
        "Channel-to-Channel only compares matching electrode labels (e.g. Fp1<->Fp1) -- "
        "requires participants to share the same channel naming/montage, or nothing will show.");
    ChannelPairingMode pairing = app_->get_channel_pairing_mode();
    if (ImGui::RadioButton("Full Matrix", pairing == ChannelPairingMode::FullMatrix)) {
        app_->set_channel_pairing_mode(ChannelPairingMode::FullMatrix);
    }
    if (ImGui::RadioButton("Channel-to-Channel", pairing == ChannelPairingMode::ChannelToChannel)) {
        app_->set_channel_pairing_mode(ChannelPairingMode::ChannelToChannel);
    }

    T::section_header("Connectivity Metric");
    P::ConnectivityMetric current = app_->get_metric();
    const P::ConnectivityMetric options[] = {
        P::ConnectivityMetric::Coherence, P::ConnectivityMetric::WPLI,
        P::ConnectivityMetric::PLV, P::ConnectivityMetric::WTC,
    };
    for (P::ConnectivityMetric m : options) {
        bool implemented = P::InterBrainConnectivityCalculator::is_metric_implemented(m);
        ImGui::BeginDisabled(!implemented);
        if (ImGui::RadioButton(P::to_metric_string(m), current == m)) {
            app_->set_metric(m);
        }
        ImGui::EndDisabled();
        if (!implemented) {
            ImGui::SameLine();
            T::text_status(T::StatusLevel::Muted, "(coming soon)");
        }
    }

    T::section_header("Frequency Band");
    struct Band { const char* name; float lo; float hi; };
    static const Band presets[] = {
        {"Theta 4-8 Hz", 4.0f, 8.0f},
        {"Alpha 8-12 Hz", 8.0f, 12.0f},
        {"Beta 12-30 Hz", 12.0f, 30.0f},
        {"Gamma 30-45 Hz", 30.0f, 45.0f},
    };
    for (const auto& band : presets) {
        bool active = (app_->get_freq_min() == band.lo && app_->get_freq_max() == band.hi);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, T::kTabActive);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        }
        if (ImGui::Button(band.name)) {
            app_->set_freq_band(band.lo, band.hi);
            custom_fmin_ = band.lo;
            custom_fmax_ = band.hi;
        }
        if (active) ImGui::PopStyleColor(2);
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Custom range (Hz):");
    ImGui::SetNextItemWidth(80);
    bool changed = ImGui::InputFloat("min##band", &custom_fmin_, 0.0f, 0.0f, "%.1f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    changed |= ImGui::InputFloat("max##band", &custom_fmax_, 0.0f, 0.0f, "%.1f");
    if (changed && custom_fmin_ > 0.0f && custom_fmax_ > custom_fmin_) {
        app_->set_freq_band(custom_fmin_, custom_fmax_);
    }

    T::section_header("Analysis Window",
        "How many samples of each participant's signal go into every coherence "
        "update. Larger = more stable estimate but slower to react.");
    int window = static_cast<int>(app_->get_window_size_samples());
    const int window_options[] = {128, 256, 512, 1024};
    for (int w : window_options) {
        if (ImGui::RadioButton((std::to_string(w) + " samples").c_str(), window == w)) {
            app_->set_window_size_samples(static_cast<size_t>(w));
        }
    }

    T::section_header("Recording & Publishing");
    bool republish = app_->get_republish_lsl_enabled();
    if (ImGui::Checkbox("Republish coherence as LSL stream (\"ACEcollective-Coherence\")", &republish)) {
        app_->set_republish_lsl_enabled(republish);
    }

    if (app_->is_recording()) {
        T::text_status(T::StatusLevel::Good, "Recording...");
        ImGui::SameLine();
        if (ImGui::Button("Stop Recording")) {
            app_->stop_recording();
        }
    } else {
        ImGui::SetNextItemWidth(160);
        ImGui::InputText("Filename", recording_filename_, sizeof(recording_filename_));
        ImGui::SameLine();
        if (ImGui::Button("Start Recording")) {
            app_->start_recording(recording_filename_);
        }
    }
    T::text_status(T::StatusLevel::Muted,
        "Recording writes a CSV timeseries of each pair's mean coherence. "
        "Full per-channel-pair matrices aren't recorded yet.");
}

} // namespace ui
} // namespace ace
