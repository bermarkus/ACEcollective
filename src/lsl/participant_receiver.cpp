#include "lsl/participant_receiver.h"

#include <algorithm>
#include <mutex>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <deque>

#ifdef LSL_ENABLED
#include <lsl_cpp.h>
#endif

namespace ace {
namespace lsl_receiving {

#ifdef LSL_ENABLED

namespace {
// How much history each participant's ring buffer keeps, regardless of the
// window size the connectivity engine asks for -- generous enough for a
// several-second coherence window at typical EEG rates without growing
// unbounded during a long session.
constexpr size_t kMaxBufferedSamples = 8192;

std::vector<std::string> read_channel_labels(lsl::stream_info& info)
{
    std::vector<std::string> labels;
    int32_t n = info.channel_count();
    lsl::xml_element ch = info.desc().child("channels").child("channel");
    while (!ch.empty()) {
        const char* label = ch.child_value("label");
        labels.push_back((label && label[0] != '\0') ? std::string(label)
                                                       : ("Ch" + std::to_string(labels.size() + 1)));
        ch = ch.next_sibling("channel");
    }
    // Pad/truncate defensively in case the advertised metadata doesn't match
    // channel_count() exactly.
    while (static_cast<int32_t>(labels.size()) < n) {
        labels.push_back("Ch" + std::to_string(labels.size() + 1));
    }
    labels.resize(n);
    return labels;
}
} // namespace

struct ParticipantSlot {
    std::string stream_name;
    std::vector<std::string> channel_names;
    double sample_rate = 0.0;
    std::unique_ptr<lsl::stream_inlet> inlet;

    mutable std::mutex buffer_mutex;
    std::vector<std::deque<float>> channel_buffers; // [channel][sample], oldest-first

    std::thread pull_thread;
    std::atomic<bool> should_stop{false};
    std::atomic<bool> connected{false};
    // Wall-clock time of the last successfully pulled sample. Guarded by
    // buffer_mutex since it's written from the pull thread and read from
    // is_connected() -- see the staleness check in pull_loop() below for why
    // this exists instead of relying only on pull_sample()/time_correction()
    // throwing lsl::lost_error.
    std::chrono::steady_clock::time_point last_sample_time;
};

struct ParticipantReceiver::Impl {
    mutable std::mutex slots_mutex;
    std::unordered_map<std::string, std::unique_ptr<ParticipantSlot>> slots;

    void pull_loop(ParticipantSlot* slot)
    {
        std::vector<float> sample(slot->channel_names.size());
        double time_correction = 0.0;
        auto last_correction = std::chrono::steady_clock::now() - std::chrono::seconds(31);

        // How long without a new sample before we call this stream dead.
        // liblsl's lost_error (thrown by pull_sample()/time_correction())
        // only fires once it has positively determined the source is gone,
        // which in practice can be slow or may not fire at all if the remote
        // process simply vanishes without a clean socket close -- pull_sample
        // just keeps returning 0.0 (plain timeout) instead. A closed ACEapp
        // instance was observed doing exactly this: no exception, ever, just
        // silent timeouts -- so staleness (no sample for N seconds) is the
        // primary disconnect signal, and a caught exception is treated the
        // same way rather than being the only path that sets it.
        constexpr auto kStaleAfter = std::chrono::seconds(3);
        {
            std::lock_guard<std::mutex> lock(slot->buffer_mutex);
            slot->last_sample_time = std::chrono::steady_clock::now();
        }

        while (!slot->should_stop.load()) {
            // Refresh clock offset periodically (cheap; LSL recommends every ~30s).
            auto now = std::chrono::steady_clock::now();
            if (now - last_correction > std::chrono::seconds(30)) {
                try {
                    time_correction = slot->inlet->time_correction(2.0);
                } catch (...) {
                    // Stream may have dropped; the staleness check below is
                    // what actually decides connected state.
                }
                last_correction = now;
            }

            double ts = 0.0;
            try {
                // Shorter timeout than before so the staleness check below
                // runs at a steady ~2Hz regardless of whether samples are
                // arriving, instead of only once per second at best.
                ts = slot->inlet->pull_sample(sample, 0.5);
            } catch (...) {
                ts = 0.0; // fall through to the staleness check
            }

            if (ts != 0.0) {
                std::lock_guard<std::mutex> lock(slot->buffer_mutex);
                slot->last_sample_time = std::chrono::steady_clock::now();
            }

            std::chrono::steady_clock::time_point last_sample;
            {
                std::lock_guard<std::mutex> lock(slot->buffer_mutex);
                last_sample = slot->last_sample_time;
            }
            slot->connected.store(std::chrono::steady_clock::now() - last_sample < kStaleAfter);

            if (ts == 0.0) {
                continue; // no new sample this iteration -- nothing to buffer
            }
            (void)time_correction; // reserved for cross-machine alignment once multiple hosts are in play

            std::lock_guard<std::mutex> lock(slot->buffer_mutex);
            for (size_t ch = 0; ch < sample.size(); ++ch) {
                auto& buf = slot->channel_buffers[ch];
                buf.push_back(sample[ch]);
                if (buf.size() > kMaxBufferedSamples) {
                    buf.pop_front();
                }
            }
        }
    }
};

ParticipantReceiver::ParticipantReceiver() : impl_(std::make_unique<Impl>()) {}

ParticipantReceiver::~ParticipantReceiver()
{
    remove_all();
}

std::vector<std::string> ParticipantReceiver::discover_streams(double wait_seconds)
{
    std::vector<std::string> names;
    try {
        auto streams = lsl::resolve_stream("type", "EEG", 0, wait_seconds);
        for (auto& s : streams) {
            names.push_back(s.name());
        }
    } catch (...) {
        // Network resolution failure -- return whatever we have (empty).
    }
    return names;
}

bool ParticipantReceiver::add_participant(const std::string& stream_name)
{
    std::lock_guard<std::mutex> lock(impl_->slots_mutex);
    if (impl_->slots.count(stream_name)) {
        return true; // already added
    }

    std::vector<lsl::stream_info> matches;
    try {
        matches = lsl::resolve_stream("name", stream_name, 1, 3.0);
    } catch (...) {
        return false;
    }
    if (matches.empty()) {
        return false;
    }

    auto slot = std::make_unique<ParticipantSlot>();
    slot->stream_name = stream_name;
    slot->channel_names = read_channel_labels(matches[0]);
    slot->sample_rate = matches[0].nominal_srate();
    slot->channel_buffers.resize(slot->channel_names.size());

    try {
        slot->inlet = std::make_unique<lsl::stream_inlet>(matches[0]);
    } catch (...) {
        return false;
    }

    ParticipantSlot* raw = slot.get();
    slot->pull_thread = std::thread([this, raw]() { impl_->pull_loop(raw); });

    impl_->slots.emplace(stream_name, std::move(slot));
    return true;
}

void ParticipantReceiver::remove_participant(const std::string& stream_name)
{
    std::unique_ptr<ParticipantSlot> slot;
    {
        std::lock_guard<std::mutex> lock(impl_->slots_mutex);
        auto it = impl_->slots.find(stream_name);
        if (it == impl_->slots.end()) return;
        slot = std::move(it->second);
        impl_->slots.erase(it);
    }
    slot->should_stop.store(true);
    if (slot->pull_thread.joinable()) {
        slot->pull_thread.join();
    }
}

void ParticipantReceiver::remove_all()
{
    std::vector<std::string> names;
    {
        std::lock_guard<std::mutex> lock(impl_->slots_mutex);
        for (auto& kv : impl_->slots) names.push_back(kv.first);
    }
    for (auto& name : names) remove_participant(name);
}

std::vector<std::string> ParticipantReceiver::get_participant_names() const
{
    std::lock_guard<std::mutex> lock(impl_->slots_mutex);
    std::vector<std::string> names;
    for (auto& kv : impl_->slots) names.push_back(kv.first);
    return names;
}

std::vector<std::string> ParticipantReceiver::get_channel_names(const std::string& stream_name) const
{
    std::lock_guard<std::mutex> lock(impl_->slots_mutex);
    auto it = impl_->slots.find(stream_name);
    if (it == impl_->slots.end()) return {};
    return it->second->channel_names;
}

double ParticipantReceiver::get_sample_rate(const std::string& stream_name) const
{
    std::lock_guard<std::mutex> lock(impl_->slots_mutex);
    auto it = impl_->slots.find(stream_name);
    if (it == impl_->slots.end()) return 0.0;
    return it->second->sample_rate;
}

bool ParticipantReceiver::is_connected(const std::string& stream_name) const
{
    std::lock_guard<std::mutex> lock(impl_->slots_mutex);
    auto it = impl_->slots.find(stream_name);
    if (it == impl_->slots.end()) return false;
    return it->second->connected.load();
}

bool ParticipantReceiver::get_window(const std::string& stream_name, size_t window_size,
                                      std::vector<std::vector<float>>& out_data) const
{
    std::lock_guard<std::mutex> lock(impl_->slots_mutex);
    auto it = impl_->slots.find(stream_name);
    if (it == impl_->slots.end()) return false;
    ParticipantSlot* slot = it->second.get();

    std::lock_guard<std::mutex> buf_lock(slot->buffer_mutex);
    if (slot->channel_buffers.empty() || slot->channel_buffers[0].size() < window_size) {
        return false;
    }
    out_data.assign(slot->channel_buffers.size(), std::vector<float>(window_size));
    for (size_t ch = 0; ch < slot->channel_buffers.size(); ++ch) {
        const auto& buf = slot->channel_buffers[ch];
        size_t start = buf.size() - window_size;
        for (size_t i = 0; i < window_size; ++i) {
            out_data[ch][i] = buf[start + i];
        }
    }
    return true;
}

#else // !LSL_ENABLED -- no-op stub so the rest of the app builds without the SDK present

struct ParticipantReceiver::Impl {};

ParticipantReceiver::ParticipantReceiver() : impl_(std::make_unique<Impl>()) {}
ParticipantReceiver::~ParticipantReceiver() = default;
std::vector<std::string> ParticipantReceiver::discover_streams(double) { return {}; }
bool ParticipantReceiver::add_participant(const std::string&) { return false; }
void ParticipantReceiver::remove_participant(const std::string&) {}
void ParticipantReceiver::remove_all() {}
std::vector<std::string> ParticipantReceiver::get_participant_names() const { return {}; }
std::vector<std::string> ParticipantReceiver::get_channel_names(const std::string&) const { return {}; }
double ParticipantReceiver::get_sample_rate(const std::string&) const { return 0.0; }
bool ParticipantReceiver::is_connected(const std::string&) const { return false; }
bool ParticipantReceiver::get_window(const std::string&, size_t, std::vector<std::vector<float>>&) const { return false; }

#endif

} // namespace lsl_receiving
} // namespace ace
