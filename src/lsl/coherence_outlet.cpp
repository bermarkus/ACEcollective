#include "lsl/coherence_outlet.h"

#include <mutex>

#ifdef LSL_ENABLED
#include <lsl_cpp.h>
#endif

namespace ace {
namespace lsl_publishing {

#ifdef LSL_ENABLED

struct CoherenceOutlet::Impl {
    std::mutex mutex;
    std::unique_ptr<lsl::stream_outlet> outlet;
    std::vector<std::string> pair_labels;
};

CoherenceOutlet::CoherenceOutlet() : impl_(std::make_unique<Impl>()) {}
CoherenceOutlet::~CoherenceOutlet() { shutdown(); }

void CoherenceOutlet::ensure_stream(const std::vector<std::string>& pair_labels)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->outlet && pair_labels == impl_->pair_labels) {
        return;
    }
    impl_->outlet.reset();
    if (pair_labels.empty()) {
        impl_->pair_labels.clear();
        return;
    }

    lsl::stream_info info("ACEcollective-Coherence", "Connectivity",
                           static_cast<int32_t>(pair_labels.size()),
                           lsl::IRREGULAR_RATE, lsl::cf_float32, "acecollective-coherence");
    lsl::xml_element channels = info.desc().append_child("channels");
    for (const auto& label : pair_labels) {
        channels.append_child("channel")
            .append_child_value("label", label)
            .append_child_value("type", "Connectivity");
    }
    impl_->outlet = std::make_unique<lsl::stream_outlet>(info);
    impl_->pair_labels = pair_labels;
}

void CoherenceOutlet::push_sample(const std::vector<float>& mean_coherence_per_pair)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->outlet || mean_coherence_per_pair.size() != impl_->pair_labels.size()) {
        return;
    }
    impl_->outlet->push_sample(mean_coherence_per_pair);
}

void CoherenceOutlet::shutdown()
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->outlet.reset();
    impl_->pair_labels.clear();
}

bool CoherenceOutlet::is_active() const
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->outlet != nullptr;
}

#else // !LSL_ENABLED

struct CoherenceOutlet::Impl {};
CoherenceOutlet::CoherenceOutlet() : impl_(std::make_unique<Impl>()) {}
CoherenceOutlet::~CoherenceOutlet() = default;
void CoherenceOutlet::ensure_stream(const std::vector<std::string>&) {}
void CoherenceOutlet::push_sample(const std::vector<float>&) {}
void CoherenceOutlet::shutdown() {}
bool CoherenceOutlet::is_active() const { return false; }

#endif

} // namespace lsl_publishing
} // namespace ace
