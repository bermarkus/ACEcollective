#pragma once

#include <memory>
#include <string>
#include <vector>
#include <utility>

namespace ace {
namespace lsl_publishing {

/**
 * @brief Republishes ACEcollective's computed inter-brain connectivity as
 * its own outgoing LSL stream, "ACEcollective-Coherence", so tools like
 * ACErecorder can log it live alongside each participant's raw EEG.
 *
 * One channel per participant pair (e.g. "Alice|Bob"), each carrying that
 * pair's mean connectivity value across their full cross-channel matrix for
 * the currently selected metric/band. The full per-channel-pair matrices are
 * denser than makes sense for a live LSL channel list -- those go to the
 * HDF5/CSV recording path instead; this stream is the live summary.
 */
class CoherenceOutlet {
public:
    CoherenceOutlet();
    ~CoherenceOutlet();

    // (Re)creates the outlet if the pair list has changed since the last call.
    void ensure_stream(const std::vector<std::string>& pair_labels);

    // One value per pair, same order as the labels passed to ensure_stream().
    void push_sample(const std::vector<float>& mean_coherence_per_pair);

    void shutdown();
    bool is_active() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace lsl_publishing
} // namespace ace
