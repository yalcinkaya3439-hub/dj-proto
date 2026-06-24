#pragma once
#include <vector>
#include <cstdint>

namespace djcore {

struct WaveformPeaks {
    std::vector<float> peaks;   // max absolute amplitude per block (mono-mixed)
    int blockSize  = 256;       // PCM samples per peak entry
    int sampleRate = 44100;
    int64_t totalSamples = 0;  // total samples per channel
    int channels   = 1;
};

class WaveformAnalyzer {
public:
    // Compute peaks from interleaved float PCM data.
    // samples: length = totalSamples * channels
    static WaveformPeaks computePeaks(
        const std::vector<float>& samples,
        int channels,
        int sampleRate,
        int blockSize = 256);

    // Resample peaks to exactly outputWidth entries covering [startSample, endSample).
    // Useful for rendering the waveform at an arbitrary pixel width / zoom level.
    static std::vector<float> getPeaksForRange(
        const WaveformPeaks& peaks,
        int64_t startSample,
        int64_t endSample,
        int outputWidth);
};

} // namespace djcore
