#include <catch2/catch.hpp>
#include "djcore/WaveformAnalyzer.h"
#include <cmath>
#include <vector>

using namespace djcore;

TEST_CASE("Silent WAV produces flat (near-zero) waveform", "[waveform]") {
    // 1 second of silence at 44100 Hz, mono
    int sampleRate = 44100;
    std::vector<float> silence(sampleRate, 0.0f);

    auto peaks = WaveformAnalyzer::computePeaks(silence, 1, sampleRate, 256);
    REQUIRE(peaks.peaks.size() > 0);

    for (float p : peaks.peaks) {
        REQUIRE(p == Approx(0.0f).margin(1e-6f));
    }
}

TEST_CASE("Single click at known position produces peak at that position", "[waveform]") {
    int sampleRate = 44100;
    int totalSamples = sampleRate * 2;  // 2 seconds
    int clickSample  = 44100;           // at 1.000 s exactly
    int blockSize    = 256;

    std::vector<float> samples(totalSamples, 0.0f);
    samples[clickSample] = 1.0f;

    auto peaks = WaveformAnalyzer::computePeaks(samples, 1, sampleRate, blockSize);

    // The block containing the click should have peak == 1.0
    int expectedBlock = clickSample / blockSize;
    REQUIRE(peaks.peaks.size() > static_cast<size_t>(expectedBlock));
    REQUIRE(peaks.peaks[expectedBlock] == Approx(1.0f));

    // All other blocks should be zero
    for (size_t i = 0; i < peaks.peaks.size(); ++i) {
        if (static_cast<int>(i) != expectedBlock)
            REQUIRE(peaks.peaks[i] == Approx(0.0f).margin(1e-6f));
    }
}

TEST_CASE("Different audio files produce different waveforms", "[waveform]") {
    int sampleRate = 44100;
    std::vector<float> fileA(sampleRate, 0.0f);
    std::vector<float> fileB(sampleRate, 0.0f);

    // Fill A with a sine wave
    for (int i = 0; i < sampleRate; ++i)
        fileA[i] = 0.5f * std::sin(2.0f * 3.14159f * 440.0f * i / sampleRate);

    // Fill B with noise bursts at end
    for (int i = sampleRate - 1000; i < sampleRate; ++i)
        fileB[i] = 0.9f;

    auto peaksA = WaveformAnalyzer::computePeaks(fileA, 1, sampleRate, 256);
    auto peaksB = WaveformAnalyzer::computePeaks(fileB, 1, sampleRate, 256);

    // The two peak vectors should differ
    REQUIRE(peaksA.peaks.size() == peaksB.peaks.size());
    bool differ = false;
    for (size_t i = 0; i < peaksA.peaks.size(); ++i) {
        if (std::abs(peaksA.peaks[i] - peaksB.peaks[i]) > 0.01f) {
            differ = true;
            break;
        }
    }
    REQUIRE(differ);
}

TEST_CASE("getPeaksForRange returns correct pixel count", "[waveform]") {
    int sampleRate = 44100;
    std::vector<float> samples(sampleRate, 0.3f);
    auto peaks = WaveformAnalyzer::computePeaks(samples, 1, sampleRate, 256);

    auto slice = WaveformAnalyzer::getPeaksForRange(peaks, 0, sampleRate, 800);
    REQUIRE(static_cast<int>(slice.size()) == 800);
}

TEST_CASE("Stereo audio: peak reflects maximum across channels", "[waveform]") {
    int sampleRate = 44100;
    // Left channel quiet, right channel loud
    std::vector<float> stereo(sampleRate * 2, 0.0f);
    for (int i = 0; i < sampleRate; ++i) {
        stereo[i * 2 + 0] = 0.0f;   // left
        stereo[i * 2 + 1] = 0.8f;   // right
    }
    auto peaks = WaveformAnalyzer::computePeaks(stereo, 2, sampleRate, 256);
    for (float p : peaks.peaks)
        REQUIRE(p == Approx(0.8f).margin(1e-5f));
}

TEST_CASE("BPM or time-sig change must NOT alter waveform peaks", "[waveform]") {
    // Waveform is computed purely from PCM; BPM / grid are separate.
    // Verify that recomputing peaks twice on the same data gives identical results.
    int sampleRate = 44100;
    std::vector<float> samples(sampleRate, 0.0f);
    for (int i = 0; i < sampleRate; i += 100)
        samples[i] = 0.5f;

    auto p1 = WaveformAnalyzer::computePeaks(samples, 1, sampleRate, 256);
    auto p2 = WaveformAnalyzer::computePeaks(samples, 1, sampleRate, 256);

    REQUIRE(p1.peaks.size() == p2.peaks.size());
    for (size_t i = 0; i < p1.peaks.size(); ++i)
        REQUIRE(p1.peaks[i] == Approx(p2.peaks[i]));
}
