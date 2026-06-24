#include "djcore/WaveformAnalyzer.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace djcore {

WaveformPeaks WaveformAnalyzer::computePeaks(
    const std::vector<float>& samples,
    int channels,
    int sampleRate,
    int blockSize)
{
    if (channels <= 0 || sampleRate <= 0 || blockSize <= 0)
        throw std::invalid_argument("WaveformAnalyzer: invalid parameters");

    WaveformPeaks result;
    result.blockSize    = blockSize;
    result.sampleRate   = sampleRate;
    result.channels     = channels;
    result.totalSamples = static_cast<int64_t>(samples.size()) / channels;

    int64_t numBlocks = (result.totalSamples + blockSize - 1) / blockSize;
    result.peaks.resize(static_cast<size_t>(numBlocks), 0.0f);

    for (int64_t b = 0; b < numBlocks; ++b) {
        int64_t start = b * blockSize;
        int64_t end   = std::min(start + blockSize, result.totalSamples);
        float   peak  = 0.0f;
        for (int64_t s = start; s < end; ++s) {
            for (int c = 0; c < channels; ++c) {
                float v = std::abs(samples[static_cast<size_t>(s * channels + c)]);
                if (v > peak) peak = v;
            }
        }
        result.peaks[static_cast<size_t>(b)] = peak;
    }

    return result;
}

std::vector<float> WaveformAnalyzer::getPeaksForRange(
    const WaveformPeaks& peaks,
    int64_t startSample,
    int64_t endSample,
    int outputWidth)
{
    if (outputWidth <= 0) return {};
    if (peaks.peaks.empty()) return std::vector<float>(outputWidth, 0.0f);

    startSample = std::max<int64_t>(0, startSample);
    endSample   = std::min(endSample, peaks.totalSamples);
    if (startSample >= endSample) return std::vector<float>(outputWidth, 0.0f);

    std::vector<float> out(outputWidth, 0.0f);
    double samplesPerPixel = static_cast<double>(endSample - startSample) / outputWidth;

    for (int px = 0; px < outputWidth; ++px) {
        double sStart = startSample + px * samplesPerPixel;
        double sEnd   = sStart + samplesPerPixel;

        int64_t blockStart = static_cast<int64_t>(sStart) / peaks.blockSize;
        int64_t blockEnd   = (static_cast<int64_t>(sEnd) + peaks.blockSize - 1) / peaks.blockSize;

        blockStart = std::max<int64_t>(0, blockStart);
        blockEnd   = std::min(blockEnd, static_cast<int64_t>(peaks.peaks.size()));

        float peak = 0.0f;
        for (int64_t b = blockStart; b < blockEnd; ++b) {
            float v = peaks.peaks[static_cast<size_t>(b)];
            if (v > peak) peak = v;
        }
        out[px] = peak;
    }
    return out;
}

} // namespace djcore
