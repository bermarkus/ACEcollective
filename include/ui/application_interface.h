#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "imgui/imgui.h"
#include "processing/interbrain_connectivity_calculator.h"

namespace ace {
namespace ui {

// One discovered/selected LSL participant stream, as shown in ConnectPanel
// and referenced by name everywhere else (matrix rows/columns, recording).
struct ParticipantStreamInfo {
    std::string lsl_stream_name;   // e.g. "ACEapp-Alice"
    std::string participant_label; // user-editable display name, defaults to lsl_stream_name
    std::vector<std::string> channel_names;
    double sample_rate = 0.0;
    bool connected = false;        // inlet opened and receiving samples
};

// How InterBrainConnectivityPanel presents the live connectivity data.
// Selected via a dropdown in SettingsPanel.
enum class ConnectivityViewType {
    PairDetail,      // one pair at a time: bipartite two-column view + pair-mean overview grid
    BlockMatrix,     // every participant's channels on one shared axis, single heatmap
    Circular,        // every participant's channels around a shared ring, chords between them
    SmallMultiples,  // one mini heatmap per pair, tiled
    ForceDirected,   // channels as nodes, coherence pulls them together
};

// Which channel pairs the connectivity engine keeps, applied when each
// pair's matrix is computed (so every view, plus recording/republishing,
// sees the same filtered result rather than each view filtering separately).
enum class ChannelPairingMode {
    FullMatrix,       // every channel of A against every channel of B (the original default)
    ChannelToChannel, // only matching electrode labels, e.g. Fp1<->Fp1 -- requires participants
                      // to share a channel naming/montage, or nothing will show
};

/**
 * Interface class that exposes Application functionality to UI panels.
 * Mirrors ACEapp's ui::ApplicationInterface pattern: avoids circular
 * dependencies between Application and Panel classes.
 */
class ApplicationInterface {
public:
    virtual ~ApplicationInterface() = default;

    virtual ImVec2 scale_vec2(float x, float y) const = 0;
    virtual float scale_value(float value) const = 0;

    // Status message system (bottom-right toast, same convention as ACEapp)
    virtual void set_status_message(const std::string& message, float duration = 2.0f) = 0;

    // -- LSL stream discovery / participant selection --------------------
    // Re-resolves ACEapp-* EEG streams currently visible on the network.
    // Populates get_discovered_streams() with the result.
    virtual void refresh_discovered_streams() = 0;
    virtual const std::vector<std::string>& get_discovered_stream_names() const = 0;

    // Add/remove a discovered stream name to/from the active participant
    // group (2-6 participants). Opens/closes the corresponding LSL inlet.
    virtual bool add_participant(const std::string& lsl_stream_name) = 0;
    virtual void remove_participant(const std::string& lsl_stream_name) = 0;
    virtual const std::vector<ParticipantStreamInfo>& get_participants() const = 0;
    virtual void set_participant_label(const std::string& lsl_stream_name, const std::string& label) = 0;

    virtual bool is_session_active() const = 0;   // >=2 participants added
    virtual void start_session() = 0;              // begin pulling + computing
    virtual void stop_session() = 0;

    // -- Connectivity engine configuration --------------------------------
    virtual ace::processing::ConnectivityMetric get_metric() const = 0;
    virtual void set_metric(ace::processing::ConnectivityMetric metric) = 0;
    virtual float get_freq_min() const = 0;
    virtual float get_freq_max() const = 0;
    virtual void set_freq_band(float fmin, float fmax) = 0;
    virtual size_t get_window_size_samples() const = 0;
    virtual void set_window_size_samples(size_t samples) = 0;

    virtual ConnectivityViewType get_view_type() const = 0;
    virtual void set_view_type(ConnectivityViewType type) = 0;

    virtual ChannelPairingMode get_channel_pairing_mode() const = 0;
    virtual void set_channel_pairing_mode(ChannelPairingMode mode) = 0;

    // Minimum connectivity value a pair must have to be drawn as a visible
    // connection (chord/line) in Circular, ForceDirected, and PairDetail --
    // does not affect BlockMatrix/SmallMultiples, which show every cell's
    // actual value as a heatmap rather than a threshold-gated line.
    virtual float get_connection_threshold() const = 0;
    virtual void set_connection_threshold(float threshold) = 0;

    // -- Live results -------------------------------------------------------
    // Cross-channel coherence matrix between two participants (by stream
    // name), rows = participant_a channels, cols = participant_b channels.
    // Empty if that pair hasn't been computed yet (e.g. not enough data).
    virtual std::vector<std::vector<float>> get_pairwise_matrix(
        const std::string& participant_a, const std::string& participant_b) const = 0;
    // Mean connectivity for a pair, scoped to whatever get_channel_pairing_mode()
    // currently means (the full matrix's mean, or just the matched-channel
    // entries' mean in ChannelToChannel mode) -- prefer this over averaging
    // get_pairwise_matrix()'s result yourself, since that matrix is
    // zero-padded for non-matching pairs in ChannelToChannel mode and a
    // naive average over it would be diluted.
    virtual float get_pairwise_mean(
        const std::string& participant_a, const std::string& participant_b) const = 0;

    // -- Recording / republishing ------------------------------------------
    virtual bool is_recording() const = 0;
    virtual void start_recording(const std::string& filename_base) = 0;
    virtual void stop_recording() = 0;
    virtual bool get_republish_lsl_enabled() const = 0;
    virtual void set_republish_lsl_enabled(bool enabled) = 0;
};

} // namespace ui
} // namespace ace
