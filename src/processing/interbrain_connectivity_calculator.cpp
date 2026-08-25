#include "processing/interbrain_connectivity_calculator.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ace {
namespace processing {

const char* to_metric_string(ConnectivityMetric metric)
{
    switch (metric) {
        case ConnectivityMetric::Coherence: return "Coherence";
        case ConnectivityMetric::WPLI:      return "WPLI";
        case ConnectivityMetric::PLV:       return "PLV";
        case ConnectivityMetric::WTC:       return "WTC";
    }
    return "Unknown";
}

bool InterBrainConnectivityCalculator::is_metric_implemented(ConnectivityMetric metric)
{
    return metric == ConnectivityMetric::Coherence || metric == ConnectivityMetric::WPLI;
}

namespace {

// Naive O(N^2) DFT -- ported as-is from ACEapp's ConnectivityCalculator
// (processing/connectivity_calculator.cpp). Fine at the window sizes used
// here (a few hundred samples) now that prepare() ensures it only runs once
// per participant per recompute cycle rather than once per pair.
std::vector<std::complex<float>> compute_fft(const std::vector<float>& signal)
{
    size_t N = signal.size();
    std::vector<std::complex<float>> result(N);
    for (size_t k = 0; k < N; ++k) {
        std::complex<float> sum(0.0f, 0.0f);
        for (size_t n = 0; n < N; ++n) {
            float angle = -2.0f * static_cast<float>(M_PI) * k * n / N;
            sum += signal[n] * std::complex<float>(std::cos(angle), std::sin(angle));
        }
        result[k] = sum;
    }
    return result;
}

float calculate_band_power(const std::vector<std::complex<float>>& fft_result,
                            float sample_rate, float freq_min, float freq_max)
{
    size_t N = fft_result.size();
    float freq_resolution = sample_rate / N;
    size_t bin_min = static_cast<size_t>(freq_min / freq_resolution);
    size_t bin_max = static_cast<size_t>(freq_max / freq_resolution);
    bin_min = std::min(bin_min, N / 2);
    bin_max = std::min(bin_max, N / 2);

    float power = 0.0f;
    for (size_t i = bin_min; i <= bin_max; ++i) {
        float magnitude = std::abs(fft_result[i]);
        power += magnitude * magnitude;
    }
    return power;
}

std::complex<float> calculate_cross_spectrum(const std::vector<std::complex<float>>& fft1,
                                              const std::vector<std::complex<float>>& fft2,
                                              float sample_rate, float freq_min, float freq_max)
{
    size_t N = fft1.size();
    float freq_resolution = sample_rate / N;
    size_t bin_min = static_cast<size_t>(freq_min / freq_resolution);
    size_t bin_max = static_cast<size_t>(freq_max / freq_resolution);
    bin_min = std::min(bin_min, N / 2);
    bin_max = std::min(bin_max, N / 2);

    std::complex<float> cross_spec(0.0f, 0.0f);
    for (size_t i = bin_min; i <= bin_max; ++i) {
        cross_spec += fft1[i] * std::conj(fft2[i]);
    }
    return cross_spec;
}

// Picks the largest power-of-two window (of a small fixed ladder, matching
// ACEapp's own choice) that fits in num_samples, or 0 if none fits.
size_t pick_window_size(size_t num_samples)
{
    for (size_t candidate : {512u, 256u, 128u}) {
        if (num_samples >= candidate) return candidate;
    }
    return 0;
}

} // namespace

InterBrainConnectivityCalculator::PreparedChannels InterBrainConnectivityCalculator::prepare(
    const std::vector<std::vector<float>>& data, float sample_rate)
{
    PreparedChannels result;
    result.sample_rate = sample_rate;
    if (data.empty() || data[0].empty()) return result;

    result.window_size = pick_window_size(data[0].size());
    if (result.window_size == 0) return result; // not enough data yet

    result.ffts.resize(data.size());
    for (size_t ch = 0; ch < data.size(); ++ch) {
        size_t num_samples = data[ch].size();
        size_t start_idx = num_samples - result.window_size;
        std::vector<float> windowed(result.window_size);
        for (size_t i = 0; i < result.window_size; ++i) {
            float hann = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * i / (result.window_size - 1)));
            windowed[i] = data[ch][start_idx + i] * hann;
        }
        result.ffts[ch] = compute_fft(windowed);
    }
    return result;
}

std::vector<std::vector<float>> InterBrainConnectivityCalculator::calculate(
    const PreparedChannels& a, const PreparedChannels& b,
    std::pair<float, float> freq_band, ConnectivityMetric metric)
{
    if (!is_metric_implemented(metric)) return {};
    if (a.window_size == 0 || b.window_size == 0) return {};

    switch (metric) {
        case ConnectivityMetric::Coherence: return calculate_coherence(a, b, freq_band);
        case ConnectivityMetric::WPLI:      return calculate_wpli(a, b, freq_band);
        default: return {};
    }
}

std::vector<std::vector<float>> InterBrainConnectivityCalculator::calculate_coherence(
    const PreparedChannels& a, const PreparedChannels& b, std::pair<float, float> freq_band)
{
    size_t num_channels_a = a.ffts.size();
    size_t num_channels_b = b.ffts.size();
    std::vector<std::vector<float>> matrix(num_channels_a, std::vector<float>(num_channels_b, 0.0f));

    for (size_t i = 0; i < num_channels_a; ++i) {
        float psd_a = calculate_band_power(a.ffts[i], a.sample_rate, freq_band.first, freq_band.second);
        for (size_t j = 0; j < num_channels_b; ++j) {
            float psd_b = calculate_band_power(b.ffts[j], b.sample_rate, freq_band.first, freq_band.second);
            std::complex<float> cross_spec = calculate_cross_spectrum(
                a.ffts[i], b.ffts[j], a.sample_rate, freq_band.first, freq_band.second);

            float coherence = 0.0f;
            if (psd_a > 0.0f && psd_b > 0.0f) {
                float cross_power = std::abs(cross_spec);
                coherence = (cross_power * cross_power) / (psd_a * psd_b);
                coherence = std::max(0.0f, std::min(1.0f, coherence));
            }
            matrix[i][j] = coherence;
        }
    }
    return matrix;
}

std::vector<std::vector<float>> InterBrainConnectivityCalculator::calculate_wpli(
    const PreparedChannels& a, const PreparedChannels& b, std::pair<float, float> freq_band)
{
    size_t num_channels_a = a.ffts.size();
    size_t num_channels_b = b.ffts.size();
    std::vector<std::vector<float>> matrix(num_channels_a, std::vector<float>(num_channels_b, 0.0f));

    size_t window_size = a.window_size; // == b.window_size (both prepared from the same window_size_samples_)
    float freq_resolution = a.sample_rate / window_size;
    size_t bin_min = static_cast<size_t>(freq_band.first / freq_resolution);
    size_t bin_max = static_cast<size_t>(freq_band.second / freq_resolution);
    bin_min = std::min(bin_min, window_size / 2);
    bin_max = std::min(bin_max, window_size / 2);

    for (size_t i = 0; i < num_channels_a; ++i) {
        for (size_t j = 0; j < num_channels_b; ++j) {
            float sum_weighted_phase_lag = 0.0f;
            float sum_abs_imaginary = 0.0f;

            for (size_t bin = bin_min; bin <= bin_max; ++bin) {
                std::complex<float> cross_spec = a.ffts[i][bin] * std::conj(b.ffts[j][bin]);
                float imaginary_part = cross_spec.imag();
                sum_weighted_phase_lag += std::abs(imaginary_part) * (imaginary_part > 0 ? 1.0f : -1.0f);
                sum_abs_imaginary += std::abs(imaginary_part);
            }

            float wpli = 0.0f;
            if (sum_abs_imaginary > 0.0f) {
                wpli = std::abs(sum_weighted_phase_lag) / sum_abs_imaginary;
                wpli = std::max(0.0f, std::min(1.0f, wpli));
            }
            matrix[i][j] = wpli;
        }
    }
    return matrix;
}

} // namespace processing
} // namespace ace
