#pragma once

#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <chrono>

#include "imgui/imgui.h"
#include "ui/application_interface.h"
#include "lsl/participant_receiver.h"
#include "lsl/coherence_outlet.h"
#include "processing/interbrain_connectivity_calculator.h"

struct GLFWwindow;

namespace ace {
namespace ui {
    class PanelManager;
}}

namespace ace {
namespace storage {
    class CoherenceCsvWriter;
}}

/**
 * @brief Main application for ACEcollective.
 *
 * Follows the same shape as ACEapp's Application class (application.h there):
 * a single class implementing ui::ApplicationInterface, owning the
 * PanelManager and the app's core engine -- here, LSL participant reception
 * plus the inter-brain connectivity calculator, instead of BrainFlow
 * acquisition plus signal processing.
 */
class Application : public ace::ui::ApplicationInterface {
public:
    Application();
    ~Application();

    void initialize();
    void update();
    void render();

    // DPI scaling (kept minimal relative to ACEapp -- see its
    // application_dpi.cpp for the full monitor-DPI detection this
    // deliberately omits for v1; ui_scale_ below covers accessibility scaling).
    ImVec2 scale_vec2(float x, float y) const override;
    float scale_value(float value) const override;

    void set_status_message(const std::string& message, float duration = 2.0f) override {
        status_message_text_ = message;
        status_message_end_time_ = std::chrono::steady_clock::now() +
                                    std::chrono::milliseconds(static_cast<int>(duration * 1000));
    }

    void refresh_discovered_streams() override;
    const std::vector<std::string>& get_discovered_stream_names() const override { return discovered_streams_; }

    bool add_participant(const std::string& lsl_stream_name) override;
    void remove_participant(const std::string& lsl_stream_name) override;
    const std::vector<ace::ui::ParticipantStreamInfo>& get_participants() const override { return participants_; }
    void set_participant_label(const std::string& lsl_stream_name, const std::string& label) override;

    bool is_session_active() const override { return session_active_; }
    void start_session() override;
    void stop_session() override;

    ace::processing::ConnectivityMetric get_metric() const override { return metric_; }
    void set_metric(ace::processing::ConnectivityMetric metric) override { metric_ = metric; }
    float get_freq_min() const override { return freq_min_; }
    float get_freq_max() const override { return freq_max_; }
    void set_freq_band(float fmin, float fmax) override { freq_min_ = fmin; freq_max_ = fmax; }
    size_t get_window_size_samples() const override { return window_size_samples_; }
    void set_window_size_samples(size_t samples) override { window_size_samples_ = samples; }

    ace::ui::ConnectivityViewType get_view_type() const override { return view_type_; }
    void set_view_type(ace::ui::ConnectivityViewType type) override { view_type_ = type; }

    ace::ui::ChannelPairingMode get_channel_pairing_mode() const override { return pairing_mode_; }
    void set_channel_pairing_mode(ace::ui::ChannelPairingMode mode) override { pairing_mode_ = mode; }

    float get_connection_threshold() const override { return connection_threshold_; }
    void set_connection_threshold(float threshold) override { connection_threshold_ = threshold; }

    std::vector<std::vector<float>> get_pairwise_matrix(
        const std::string& participant_a, const std::string& participant_b) const override;
    float get_pairwise_mean(
        const std::string& participant_a, const std::string& participant_b) const override;

    bool is_recording() const override { return is_recording_; }
    void start_recording(const std::string& filename_base) override;
    void stop_recording() override;
    bool get_republish_lsl_enabled() const override { return republish_lsl_enabled_; }
    void set_republish_lsl_enabled(bool enabled) override { republish_lsl_enabled_ = enabled; }

    // Called once per frame from update(): pulls the latest window for every
    // active participant pair and recomputes their matrices. Runs on the
    // main thread for v1 -- see docs note in application.cpp about moving
    // this to a worker thread once participant/channel counts make it a
    // frame-time concern (mirrors ACEapp's ConnectivityPanel background
    // thread, which this deliberately doesn't copy yet).
    void recompute_all_pairs();

private:
    // Refreshes ParticipantStreamInfo::connected from the receiver's live
    // state and auto-removes anyone who has genuinely dropped out (their
    // ACEapp instance closed) -- see the definition for why this runs
    // unconditionally in update() rather than only inside recompute_all_pairs().
    void refresh_connection_states();

    std::unique_ptr<ace::ui::PanelManager> panel_manager_;
    ace::lsl_receiving::ParticipantReceiver receiver_;
    ace::processing::InterBrainConnectivityCalculator calculator_;
    ace::lsl_publishing::CoherenceOutlet coherence_outlet_;

    std::vector<std::string> discovered_streams_;
    std::vector<ace::ui::ParticipantStreamInfo> participants_;
    bool session_active_ = false;

    ace::processing::ConnectivityMetric metric_ = ace::processing::ConnectivityMetric::Coherence;
    float freq_min_ = 8.0f;
    float freq_max_ = 12.0f;
    size_t window_size_samples_ = 512;
    ace::ui::ConnectivityViewType view_type_ = ace::ui::ConnectivityViewType::PairDetail;
    ace::ui::ChannelPairingMode pairing_mode_ = ace::ui::ChannelPairingMode::FullMatrix;
    float connection_threshold_ = 0.7f;

    // Latest cross matrix + its correctly-scoped mean for each pair (mean is
    // computed over only the entries that count for the current pairing
    // mode -- see recompute_all_pairs() -- so callers never need to
    // recompute a mean from an already-filtered, zero-padded matrix
    // themselves; that would dilute the average in ChannelToChannel mode).
    struct PairwiseResult {
        std::string key; // pair_key(participants_[i], participants_[j]), i<j in participants_ order
        std::vector<std::vector<float>> matrix;
        float mean = 0.0f;
    };
    mutable std::mutex matrices_mutex_;
    std::vector<PairwiseResult> pairwise_matrices_;

    bool is_recording_ = false;
    std::unique_ptr<ace::storage::CoherenceCsvWriter> csv_writer_;
    bool republish_lsl_enabled_ = false;

    std::string status_message_text_;
    std::chrono::steady_clock::time_point status_message_end_time_;

    float ui_scale_ = 1.0f;

    std::string pair_key(const std::string& a, const std::string& b) const { return a + "|" + b; }
};
