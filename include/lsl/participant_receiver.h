#pragma once

#include <memory>
#include <string>
#include <vector>

namespace ace {
namespace lsl_receiving {

/**
 * @brief Discovers ACEapp EEG streams on the network and manages one LSL
 * inlet + pull thread per selected participant (2-6 for a group session).
 *
 * Mirrors ACEapp's lsl::LslStreamer (the outlet side, src/lsl/lsl_streamer.cpp)
 * in spirit: a thin RAII wrapper around the LSL SDK with a no-op stub when
 * LSL_ENABLED isn't defined, so the rest of the app still builds without the
 * SDK present.
 */
class ParticipantReceiver {
public:
    ParticipantReceiver();
    ~ParticipantReceiver();

    // Re-resolves all currently-visible LSL streams of type "EEG" (what
    // ACEapp's LslStreamer advertises -- see its ensure_streams()).
    // wait_seconds is how long to listen for stream announcements.
    static std::vector<std::string> discover_streams(double wait_seconds = 1.0);

    // Opens an inlet for a discovered stream name and starts pulling samples
    // into an internal ring buffer on a dedicated thread. Returns false if
    // the stream can no longer be resolved or an inlet couldn't be opened.
    bool add_participant(const std::string& stream_name);
    void remove_participant(const std::string& stream_name);
    void remove_all();

    std::vector<std::string> get_participant_names() const;
    std::vector<std::string> get_channel_names(const std::string& stream_name) const;
    double get_sample_rate(const std::string& stream_name) const;
    bool is_connected(const std::string& stream_name) const;

    // Snapshot of the trailing window_size samples per channel (newest last).
    // Returns false if fewer than window_size samples have been buffered yet.
    // out_data is [num_channels][window_size].
    bool get_window(const std::string& stream_name, size_t window_size,
                     std::vector<std::vector<float>>& out_data) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace lsl_receiving
} // namespace ace
