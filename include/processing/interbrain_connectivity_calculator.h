#pragma once

#include <vector>
#include <string>
#include <utility>
#include <complex>
#include <cstddef>

namespace ace {
namespace processing {

// Cross-participant connectivity metric. This list is deliberately larger
// than what's implemented today: the engine's call surface (calculate())
// takes a metric enum from day one so WPLI/PLV/WTC can be added later
// without changing every call site that already depends on this API.
// Check is_metric_implemented() before relying on a result.
enum class ConnectivityMetric {
    Coherence, // magnitude-squared coherence -- matches ACEanalysis's offline 'coh'
    WPLI,      // weighted phase-lag index -- ported from ACEapp's intra-brain version
    PLV,       // phase-locking value -- not yet implemented
    WTC,       // wavelet transform coherence -- not yet implemented
};

const char* to_metric_string(ConnectivityMetric metric);

/**
 * @brief Calculates connectivity metrics between two participants' EEG
 * channel sets (inter-brain / hyperscanning coherence).
 *
 * This is the cross-participant generalization of ACEapp's single-subject
 * ConnectivityCalculator (processing/connectivity_calculator.h there):
 * instead of an NxN matrix over one person's own channels, calculate()
 * takes two independent channel sets -- e.g. participant A's 32 channels and
 * participant B's 32 channels -- and returns a channels_a x channels_b cross
 * matrix, since ACEcollective compares the full cross-channel matrix between
 * every pair of participants rather than only homologous electrode sites.
 *
 * Split into prepare() + calculate(prepared, prepared, ...) rather than one
 * combined call: the windowed per-channel FFT (prepare()'s job) is the
 * expensive O(window_size^2) part, and a naive one-call-per-pair API would
 * redo it for the same participant on every pair they appear in -- for an
 * N-participant session that's roughly (N-1)x more FFT work than necessary,
 * and was the direct cause of a visible per-tick UI stutter once several
 * participants were connected. Callers should prepare() each participant
 * exactly once per recompute cycle and reuse the result across every pair.
 */
class InterBrainConnectivityCalculator {
public:
    InterBrainConnectivityCalculator() = default;

    struct PreparedChannels {
        std::vector<std::vector<std::complex<float>>> ffts; // [channel][freq bin]
        float sample_rate = 0.0f;
        size_t window_size = 0; // 0 means "not enough samples" -- calculate() returns empty
    };

    // data: [num_channels][num_samples]. Windows, Hann-tapers, and FFTs
    // every channel once.
    static PreparedChannels prepare(const std::vector<std::vector<float>>& data, float sample_rate);

    // Cross matrix (a.ffts.size() x b.ffts.size()) from two already-prepared
    // participants. Returns an empty vector if the requested metric isn't
    // implemented yet or either side lacks enough data.
    std::vector<std::vector<float>> calculate(
        const PreparedChannels& a, const PreparedChannels& b,
        std::pair<float, float> freq_band, ConnectivityMetric metric);

    static bool is_metric_implemented(ConnectivityMetric metric);

private:
    std::vector<std::vector<float>> calculate_coherence(
        const PreparedChannels& a, const PreparedChannels& b, std::pair<float, float> freq_band);

    std::vector<std::vector<float>> calculate_wpli(
        const PreparedChannels& a, const PreparedChannels& b, std::pair<float, float> freq_band);
};

} // namespace processing
} // namespace ace
