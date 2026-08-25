#include "storage/coherence_csv_writer.h"

namespace ace {
namespace storage {

CoherenceCsvWriter::~CoherenceCsvWriter()
{
    stop();
}

bool CoherenceCsvWriter::start(const std::string& filepath, const std::vector<std::string>& pair_labels)
{
    std::lock_guard<std::mutex> lock(mutex_);
    file_.open(filepath, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) {
        return false;
    }
    num_pairs_ = pair_labels.size();

    file_ << "timestamp_s";
    for (const auto& label : pair_labels) {
        file_ << "," << label;
    }
    file_ << "\n";
    file_.flush();
    return true;
}

void CoherenceCsvWriter::write_row(double timestamp_s, const std::vector<float>& mean_coherence_per_pair)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!file_.is_open() || mean_coherence_per_pair.size() != num_pairs_) {
        return;
    }
    file_ << timestamp_s;
    for (float v : mean_coherence_per_pair) {
        file_ << "," << v;
    }
    file_ << "\n";
    file_.flush();
}

void CoherenceCsvWriter::stop()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        file_.close();
    }
    num_pairs_ = 0;
}

bool CoherenceCsvWriter::is_open() const
{
    return file_.is_open();
}

} // namespace storage
} // namespace ace
