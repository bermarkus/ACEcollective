#include "application.h"

#include <algorithm>
#include <chrono>
#include <filesystem>

#include "ui/panel_manager.h"
#include "storage/coherence_csv_writer.h"
#include "utils/app_paths.h"

namespace {
float mean_of(const std::vector<std::vector<float>>& matrix)
{
    if (matrix.empty() || matrix[0].empty()) return 0.0f;
    double sum = 0.0;
    size_t n = 0;
    for (const auto& row : matrix) {
        for (float v : row) { sum += v; ++n; }
    }
    return n > 0 ? static_cast<float>(sum / n) : 0.0f;
}

// Zeroes every entry of `matrix` except where channel names match between
// the two participants (e.g. Fp1<->Fp1) -- the ChannelToChannel pairing
// mode. Returns the mean over only the matched entries (0 if none matched,
// e.g. the two participants don't share a channel naming/montage), not a
// mean diluted by all the zeroed-out entries.
float filter_to_channel_to_channel(std::vector<std::vector<float>>& matrix,
                                    const std::vector<std::string>& names_a,
                                    const std::vector<std::string>& names_b)
{
    double sum = 0.0;
    size_t matched = 0;
    for (size_t i = 0; i < matrix.size(); ++i) {
        for (size_t j = 0; j < matrix[i].size(); ++j) {
            bool same_label = i < names_a.size() && j < names_b.size() && names_a[i] == names_b[j];
            if (same_label) {
                sum += matrix[i][j];
                ++matched;
            } else {
                matrix[i][j] = 0.0f;
            }
        }
    }
    return matched > 0 ? static_cast<float>(sum / matched) : 0.0f;
}
} // namespace

Application::Application()
    : csv_writer_(std::make_unique<ace::storage::CoherenceCsvWriter>())
{
}

Application::~Application()
{
    stop_session();
}

void Application::initialize()
{
    panel_manager_ = std::make_unique<ace::ui::PanelManager>(this);
    panel_manager_->initialize();
    refresh_discovered_streams();
}

ImVec2 Application::scale_vec2(float x, float y) const
{
    return ImVec2(x * ui_scale_, y * ui_scale_);
}

float Application::scale_value(float value) const
{
    return value * ui_scale_;
}

void Application::refresh_discovered_streams()
{
    discovered_streams_ = ace::lsl_receiving::ParticipantReceiver::discover_streams(1.0);
}

bool Application::add_participant(const std::string& lsl_stream_name)
{
    if (participants_.size() >= 6) {
        set_status_message("Maximum of 6 participants per session", 3.0f);
        return false;
    }
    for (const auto& p : participants_) {
        if (p.lsl_stream_name == lsl_stream_name) return true; // already added
    }

    if (!receiver_.add_participant(lsl_stream_name)) {
        set_status_message("Could not connect to stream: " + lsl_stream_name, 3.0f);
        return false;
    }

    ace::ui::ParticipantStreamInfo info;
    info.lsl_stream_name = lsl_stream_name;
    info.participant_label = lsl_stream_name;
    info.channel_names = receiver_.get_channel_names(lsl_stream_name);
    info.sample_rate = receiver_.get_sample_rate(lsl_stream_name);
    // Not actually connected yet -- the inlet is open, but connected only
    // becomes true once the pull thread confirms a real sample/time_correction
    // (see recompute_all_pairs(), which refreshes this every tick from
    // receiver_.is_connected()). Starting this true regardless of what the
    // receiver was reporting was the root cause of a stale "connected"
    // participant never dropping out after its ACEapp instance closed.
    info.connected = false;
    participants_.push_back(info);

    session_active_ = participants_.size() >= 2;
    return true;
}

void Application::remove_participant(const std::string& lsl_stream_name)
{
    receiver_.remove_participant(lsl_stream_name);
    participants_.erase(
        std::remove_if(participants_.begin(), participants_.end(),
            [&](const ace::ui::ParticipantStreamInfo& p) { return p.lsl_stream_name == lsl_stream_name; }),
        participants_.end());

    {
        std::lock_guard<std::mutex> lock(matrices_mutex_);
        pairwise_matrices_.erase(
            std::remove_if(pairwise_matrices_.begin(), pairwise_matrices_.end(),
                [&](const PairwiseResult& result) {
                    return result.key.find(lsl_stream_name) != std::string::npos;
                }),
            pairwise_matrices_.end());
    }

    session_active_ = participants_.size() >= 2;
}

void Application::set_participant_label(const std::string& lsl_stream_name, const std::string& label)
{
    for (auto& p : participants_) {
        if (p.lsl_stream_name == lsl_stream_name) {
            p.participant_label = label.empty() ? lsl_stream_name : label;
            return;
        }
    }
}

void Application::start_session()
{
    session_active_ = participants_.size() >= 2;
    if (!session_active_) {
        set_status_message("Add at least 2 participants to start a session", 3.0f);
    }
}

void Application::stop_session()
{
    if (is_recording_) stop_recording();
    coherence_outlet_.shutdown();
    receiver_.remove_all();
    participants_.clear();
    {
        std::lock_guard<std::mutex> lock(matrices_mutex_);
        pairwise_matrices_.clear();
    }
    session_active_ = false;
}

std::vector<std::vector<float>> Application::get_pairwise_matrix(
    const std::string& participant_a, const std::string& participant_b) const
{
    std::lock_guard<std::mutex> lock(matrices_mutex_);
    std::string key_ab = pair_key(participant_a, participant_b);
    for (const auto& result : pairwise_matrices_) {
        if (result.key == key_ab) return result.matrix;
    }
    // Try the reverse order and transpose.
    std::string key_ba = pair_key(participant_b, participant_a);
    for (const auto& result : pairwise_matrices_) {
        if (result.key == key_ba) {
            const auto& m = result.matrix;
            if (m.empty()) return {};
            std::vector<std::vector<float>> transposed(m[0].size(), std::vector<float>(m.size()));
            for (size_t i = 0; i < m.size(); ++i)
                for (size_t j = 0; j < m[i].size(); ++j)
                    transposed[j][i] = m[i][j];
            return transposed;
        }
    }
    return {};
}

float Application::get_pairwise_mean(
    const std::string& participant_a, const std::string& participant_b) const
{
    std::lock_guard<std::mutex> lock(matrices_mutex_);
    std::string key_ab = pair_key(participant_a, participant_b);
    std::string key_ba = pair_key(participant_b, participant_a);
    for (const auto& result : pairwise_matrices_) {
        if (result.key == key_ab || result.key == key_ba) return result.mean;
    }
    return 0.0f;
}

void Application::start_recording(const std::string& filename_base)
{
    if (participants_.size() < 2) {
        set_status_message("Add at least 2 participants before recording", 3.0f);
        return;
    }
    std::vector<std::string> pair_labels;
    for (size_t i = 0; i < participants_.size(); ++i) {
        for (size_t j = i + 1; j < participants_.size(); ++j) {
            pair_labels.push_back(participants_[i].participant_label + "|" + participants_[j].participant_label);
        }
    }

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time_t);
#else
    localtime_r(&time_t, &tm);
#endif
    char timestamp[32];
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%dT%H%M%S", &tm);

    std::string path = ace::utils::get_recordings_directory() + "/" +
                        filename_base + "_" + timestamp + ".csv";
    if (csv_writer_->start(path, pair_labels)) {
        is_recording_ = true;
        set_status_message("Recording to " + path, 3.0f);
    } else {
        set_status_message("Failed to open recording file: " + path, 4.0f);
    }
}

void Application::stop_recording()
{
    csv_writer_->stop();
    is_recording_ = false;
}

void Application::refresh_connection_states()
{
    // Runs every tick regardless of session_active_, so a lone just-added
    // participant's status moves from "waiting" to "connected" as soon as
    // its pull thread confirms a real sample -- not just once a 2nd
    // participant starts the session and recompute_all_pairs() begins
    // running. Detects genuine drops (was connected, now isn't) the same
    // way regardless of session state; removal is queued and applied after
    // the loop so mutating participants_ mid-iteration is never in play.
    std::vector<std::string> lost_participants;
    for (auto& p : participants_) {
        bool now_connected = receiver_.is_connected(p.lsl_stream_name);
        if (p.connected && !now_connected) {
            lost_participants.push_back(p.lsl_stream_name);
        }
        p.connected = now_connected;
    }
    for (const auto& name : lost_participants) {
        set_status_message(name + " disconnected -- removed from session", 4.0f);
        remove_participant(name);
    }
}

void Application::recompute_all_pairs()
{
    if (!session_active_ || participants_.size() < 2) return;

    // Prepare (window + Hann-taper + FFT) each connected participant's
    // channels exactly once and reuse across every pair they appear in --
    // calculate() used to redo this per pair, which for an N-participant
    // session meant each participant's FFTs were recomputed (N-1) times per
    // tick. That redundant O(window_size^2) work was the direct cause of a
    // visible per-tick UI stutter once several participants were connected.
    using Prepared = ace::processing::InterBrainConnectivityCalculator::PreparedChannels;
    std::vector<Prepared> prepared(participants_.size());
    std::vector<bool> has_data(participants_.size(), false);

    for (size_t i = 0; i < participants_.size(); ++i) {
        if (!participants_[i].connected) continue; // dropped or not yet live -- excluded from computation

        std::vector<std::vector<float>> data;
        if (!receiver_.get_window(participants_[i].lsl_stream_name, window_size_samples_, data)) continue;

        float sample_rate = static_cast<float>(participants_[i].sample_rate);
        if (sample_rate <= 0.0f) continue;

        prepared[i] = calculator_.prepare(data, sample_rate);
        has_data[i] = prepared[i].window_size > 0;
    }

    std::vector<PairwiseResult> fresh;
    std::vector<std::string> pair_labels;
    std::vector<float> pair_means;

    for (size_t i = 0; i < participants_.size(); ++i) {
        if (!has_data[i]) continue;

        for (size_t j = i + 1; j < participants_.size(); ++j) {
            if (!has_data[j]) continue;

            auto matrix = calculator_.calculate(prepared[i], prepared[j], {freq_min_, freq_max_}, metric_);
            if (matrix.empty()) continue;

            float pair_mean;
            if (pairing_mode_ == ace::ui::ChannelPairingMode::ChannelToChannel) {
                // Zeroes matrix in place except where channel names match
                // (e.g. Fp1<->Fp1), and returns the mean over only those
                // matched entries.
                pair_mean = filter_to_channel_to_channel(matrix, participants_[i].channel_names,
                                                          participants_[j].channel_names);
            } else {
                pair_mean = mean_of(matrix);
            }

            std::string key = pair_key(participants_[i].lsl_stream_name, participants_[j].lsl_stream_name);
            fresh.push_back({key, matrix, pair_mean});

            pair_labels.push_back(participants_[i].participant_label + "|" + participants_[j].participant_label);
            pair_means.push_back(pair_mean);
        }
    }

    {
        std::lock_guard<std::mutex> lock(matrices_mutex_);
        pairwise_matrices_ = std::move(fresh);
    }

    if (republish_lsl_enabled_ && !pair_labels.empty()) {
        coherence_outlet_.ensure_stream(pair_labels);
        coherence_outlet_.push_sample(pair_means);
    }
    if (is_recording_ && !pair_means.empty()) {
        double now_s = std::chrono::duration<double>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        csv_writer_->write_row(now_s, pair_means);
    }
}

void Application::update()
{
    refresh_connection_states();

    // Recomputed on a fixed cadence rather than every frame -- matches
    // ACEapp's ConnectivityPanel (update_interval_ = 0.5f there), and keeps
    // the per-pair FFT work off the UI's frame budget as participant/channel
    // counts grow. Runs inline on the UI thread for v1; move to a background
    // thread (as ConnectivityPanel does) if profiling shows frame hitches
    // once several 32-channel participants are all active at once.
    static auto last_recompute = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<float>(now - last_recompute).count() >= 0.5f) {
        recompute_all_pairs();
        last_recompute = now;
    }
}

void Application::render()
{
    panel_manager_->render_panels(nullptr, nullptr);
}
