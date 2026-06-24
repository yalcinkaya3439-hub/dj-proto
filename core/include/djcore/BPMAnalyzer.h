#pragma once
#include <vector>

namespace djcore {

enum class BPMConfidence { High, Medium, Low };

struct BPMResult {
    double bpm             = 0.0;
    double firstBeatSample = 0.0;  // estimated first strong beat (samples from file start)
    BPMConfidence confidence = BPMConfidence::Low;
    double confidenceScore = 0.0;  // 0..1
};

class BPMAnalyzer {
public:
    // Analyze interleaved float PCM data and return BPM estimate.
    // Uses spectral flux onset detection + autocorrelation.
    // BPM range searched: 60 – 220 (before any /2 or x2 correction).
    static BPMResult analyze(
        const std::vector<float>& samples,
        int channels,
        int sampleRate);

private:
    // Mix to mono and return mono sample vector
    static std::vector<float> toMono(
        const std::vector<float>& samples,
        int channels);

    // Compute half-wave-rectified spectral flux onset envelope, one value per hop
    static std::vector<float> computeOnsetEnvelope(
        const std::vector<float>& mono,
        int sampleRate,
        int frameSize,
        int hopSize);

    // Autocorrelation-based tempo estimation on onset envelope
    // Returns {bpm, confidenceScore}
    static std::pair<double, double> autocorrelationBPM(
        const std::vector<float>& onset,
        int sampleRate,
        int hopSize,
        double minBPM,
        double maxBPM);

    // Find position of strongest onset peak (→ first beat estimate)
    static double estimateFirstBeat(
        const std::vector<float>& onset,
        int hopSize,
        double beatPeriodSamples);
};

} // namespace djcore
