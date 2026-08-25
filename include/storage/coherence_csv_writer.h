#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <mutex>

namespace ace {
namespace storage {

/**
 * @brief Writes computed inter-brain connectivity to a CSV timeseries: one
 * row per update, one column per participant pair, holding that pair's mean
 * connectivity value across its full cross-channel matrix.
 *
 * This is the pragmatic v1 recording format -- ACEapp's own C++ side has no
 * HDF5 writer to build on (only CSV/BDF/BrainVision), and ACEanalysis's
 * offline HDF5 schema (see coherence_h5_saver.py there) is Python/h5py-based
 * with no equivalent C++ dependency already vendored anywhere in the ACEapp
 * family. Matching that schema in C++ would mean vendoring a new HDF5
 * dependency across both platforms -- worth doing once the metric/window
 * choices stabilize, but not blocking the first working recording path.
 * Per-channel-pair matrices (not just the pair mean) can be added as
 * additional CSV columns or a second file without changing this class's
 * shape.
 */
class CoherenceCsvWriter {
public:
    CoherenceCsvWriter() = default;
    ~CoherenceCsvWriter();

    // pair_labels are the CSV column headers (after "timestamp_s"), e.g. "Alice|Bob".
    bool start(const std::string& filepath, const std::vector<std::string>& pair_labels);
    void write_row(double timestamp_s, const std::vector<float>& mean_coherence_per_pair);
    void stop();
    bool is_open() const;

private:
    std::mutex mutex_;
    std::ofstream file_;
    size_t num_pairs_ = 0;
};

} // namespace storage
} // namespace ace
