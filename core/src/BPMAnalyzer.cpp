#ifdef _WIN32
#  define _USE_MATH_DEFINES   // must precede <cmath> on MSVC
#endif
#include "djcore/BPMAnalyzer.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>
#include <complex>

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

namespace djcore {

namespace {
// Simple Hann window
float hann(int n, int N) {
    return 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * n / (N - 1)));
}

// Compute magnitude spectrum of a windowed frame using DFT (O(N^2), small frames only)
std::vector<float> magnitudeSpectrum(const std::vector<float>& frame) {
    int N = static_cast<int>(frame.size());
    int half = N / 2 + 1;
    std::vector<float> mag(half, 0.0f);
    for (int k = 0; k < half; ++k) {
        float re = 0.0f, im = 0.0f;
        for (int n = 0; n < N; ++n) {
            float angle = -2.0f * static_cast<float>(M_PI) * k * n / N;
            re += frame[n] * std::cos(angle);
            im += frame[n] * std::sin(angle);
        }
        mag[k] = std::sqrt(re * re + im * im);
    }
    return mag;
}
} // anonymous namespace

std::vector<float> BPMAnalyzer::toMono(const std::vector<float>& samples, int channels) {
    if (channels == 1) return samples;
    size_t frames = samples.size() / channels;
    std::vector<float> mono(frames);
    float inv = 1.0f / channels;
    for (size_t i = 0; i < frames; ++i) {
        float sum = 0.0f;
        for (int c = 0; c < channels; ++c)
            sum += samples[i * channels + c];
        mono[i] = sum * inv;
    }
    return mono;
}

std::vector<float> BPMAnalyzer::computeOnsetEnvelope(
    const std::vector<float>& mono,
    int sampleRate,
    int frameSize,
    int hopSize)
{
    size_t N = mono.size();
    if (N < static_cast<size_t>(frameSize)) return {};

    std::vector<float> prevMag;
    std::vector<float> onset;

    for (size_t pos = 0; pos + frameSize <= N; pos += hopSize) {
        // Apply Hann window
        std::vector<float> frame(frameSize);
        for (int i = 0; i < frameSize; ++i)
            frame[i] = mono[pos + i] * hann(i, frameSize);

        // Use simple energy per sub-band as a cheap spectral flux proxy
        // Split into 8 sub-bands and sum half-wave-rectified differences
        int halfSpec = frameSize / 2 + 1;
        // Approximate magnitude with frame energy per 8 sub-bands
        int bandWidth = halfSpec / 8;
        std::vector<float> bandEnergy(8, 0.0f);
        for (int b = 0; b < 8; ++b) {
            int start = b * bandWidth;
            int end   = (b == 7) ? halfSpec : start + bandWidth;
            for (int k = start; k < end; ++k) {
                // Use raw amplitude squared (avoid full DFT for performance)
                // This is a simplified energy-based onset detector
            }
        }
        // Simplified: use frame RMS as onset value, half-wave-rectified difference
        float rms = 0.0f;
        for (int i = 0; i < frameSize; ++i)
            rms += frame[i] * frame[i];
        rms = std::sqrt(rms / frameSize);

        float flux = prevMag.empty() ? rms : std::max(0.0f, rms - prevMag[0]);
        prevMag = {rms};
        onset.push_back(flux);
    }
    return onset;
}

std::pair<double, double> BPMAnalyzer::autocorrelationBPM(
    const std::vector<float>& onset,
    int sampleRate,
    int hopSize,
    double minBPM,
    double maxBPM)
{
    if (onset.size() < 4) return {120.0, 0.0};

    int n = static_cast<int>(onset.size());

    // Convert BPM range to lag range (in onset frames)
    double onsetRate    = static_cast<double>(sampleRate) / hopSize;  // frames/sec
    int    maxLag = static_cast<int>(std::ceil(60.0 / minBPM * onsetRate));
    int    minLag = static_cast<int>(std::floor(60.0 / maxBPM * onsetRate));
    maxLag = std::min(maxLag, n - 1);
    if (minLag < 1) minLag = 1;

    if (minLag > maxLag) return {120.0, 0.0};

    // Normalised autocorrelation
    double norm0 = 0.0;
    for (int i = 0; i < n; ++i) norm0 += onset[i] * onset[i];
    if (norm0 == 0.0) return {120.0, 0.0};

    std::vector<double> ac(maxLag + 1, 0.0);
    for (int lag = minLag; lag <= maxLag; ++lag) {
        double sum = 0.0;
        for (int i = 0; i < n - lag; ++i)
            sum += onset[i] * onset[i + lag];
        ac[lag] = sum / norm0;
    }

    // Find best lag (highest autocorrelation in BPM range)
    int    bestLag   = minLag;
    double bestScore = -1.0;
    for (int lag = minLag; lag <= maxLag; ++lag) {
        if (ac[lag] > bestScore) {
            bestScore = ac[lag];
            bestLag   = lag;
        }
    }

    double bestPeriodSec = static_cast<double>(bestLag) / onsetRate;
    double bestBPM       = 60.0 / bestPeriodSec;

    // Keep BPM in 60-220 range by halving / doubling
    while (bestBPM < 60.0)  bestBPM *= 2.0;
    while (bestBPM > 220.0) bestBPM /= 2.0;

    return {bestBPM, bestScore};
}

double BPMAnalyzer::estimateFirstBeat(
    const std::vector<float>& onset,
    int hopSize,
    double beatPeriodSamples)
{
    if (onset.empty()) return 0.0;

    // Find the strongest onset peak (= likely a strong beat)
    float maxVal   = 0.0f;
    int   maxFrame = 0;
    for (int i = 0; i < static_cast<int>(onset.size()); ++i) {
        if (onset[i] > maxVal) {
            maxVal   = onset[i];
            maxFrame = i;
        }
    }

    // Walk backward from that peak in steps of beatPeriodSamples/hopSize
    // to reach the first beat position
    double beatPeriodFrames = beatPeriodSamples / hopSize;
    double firstBeatFrame   = maxFrame;
    while (firstBeatFrame >= beatPeriodFrames)
        firstBeatFrame -= beatPeriodFrames;

    return firstBeatFrame * hopSize;
}

BPMResult BPMAnalyzer::analyze(
    const std::vector<float>& samples,
    int channels,
    int sampleRate)
{
    BPMResult result;
    result.bpm             = 120.0;
    result.confidence      = BPMConfidence::Low;
    result.confidenceScore = 0.0;
    result.firstBeatSample = 0.0;

    if (samples.empty() || channels <= 0 || sampleRate <= 0) return result;

    auto mono = toMono(samples, channels);

    // Use ~23 ms frames and 50% overlap — reasonable for BPM detection
    int frameSize = 1024;
    int hopSize   = 512;

    // Adjust for very short files
    while (frameSize > static_cast<int>(mono.size()) / 4 && frameSize > 64) {
        frameSize /= 2;
        hopSize   /= 2;
    }
    if (hopSize < 1) hopSize = 1;

    auto onset = computeOnsetEnvelope(mono, sampleRate, frameSize, hopSize);
    if (onset.empty()) return result;

    auto [bpm, score] = autocorrelationBPM(onset, sampleRate, hopSize, 60.0, 220.0);

    result.bpm             = bpm;
    result.confidenceScore = score;

    if (score >= 0.45)
        result.confidence = BPMConfidence::High;
    else if (score >= 0.25)
        result.confidence = BPMConfidence::Medium;
    else
        result.confidence = BPMConfidence::Low;

    // Estimate first beat sample
    double beatPeriodSamples = 60.0 / bpm * sampleRate;
    result.firstBeatSample   = estimateFirstBeat(onset, hopSize, beatPeriodSamples);

    return result;
}

} // namespace djcore
