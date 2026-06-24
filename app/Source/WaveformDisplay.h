#pragma once
#include <JuceHeader.h>
#include "djcore/WaveformAnalyzer.h"
#include "djcore/BeatGrid.h"

// ---- WaveformDisplay -------------------------------------------------------
// Custom JUCE component that draws:
//   Layer 1: Real PCM peak waveform
//   Layer 2: Sub-beat grid lines (thin)
//   Layer 3: Group-start grid lines (medium)
//   Layer 4: Measure-start grid lines (thick)
//   Layer 5: Phrase-start grid lines (most prominent)
//   Layer 6: Cue and loop markers
//   Layer 7: Playhead (tied to actual audio sample position)
//
// The waveform shape NEVER changes when BPM/time-sig changes.
// Only the grid overlay is recomputed.

class WaveformDisplay : public juce::Component,
                        private juce::Timer
{
public:
    WaveformDisplay();
    ~WaveformDisplay() override;

    // Set data — call after deck loads a file
    void setWaveformData(const djcore::WaveformPeaks* peaks,
                         const djcore::BeatGrid*      grid,
                         const std::atomic<double>*   playheadSamplePtr,
                         int64_t                      totalSamples);

    void clear();

    // Zoom: visible region in samples
    void setVisibleRange(int64_t startSample, int64_t endSample);
    void zoomIn();
    void zoomOut();
    void centerOnPlayhead();

    // Cue / loop markers
    void setLoopRegion(double startSample, double endSample, bool active);
    void setCuePoint(double sample);

    // Callbacks
    std::function<void(double samplePos)> onSeek;   // user clicked on waveform

    // JUCE Component
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e,
                        const juce::MouseWheelDetails& wheel) override;

private:
    void timerCallback() override;

    double sampleToX(double sample) const;
    double xToSample(float x)       const;

    const djcore::WaveformPeaks* peaks_         = nullptr;
    const djcore::BeatGrid*      grid_          = nullptr;
    const std::atomic<double>*   playheadPtr_   = nullptr;
    int64_t                      totalSamples_  = 0;

    int64_t visibleStart_ = 0;
    int64_t visibleEnd_   = 0;
    int64_t defaultWindowSamples_ = 44100 * 4;  // 4 seconds at default zoom

    double loopStartSample_ = 0.0;
    double loopEndSample_   = 0.0;
    bool   loopActive_      = false;
    double cuePoint_        = 0.0;

    // Cached waveform pixel data (invalidated on resize / zoom change)
    juce::Image waveformCache_;
    bool        cacheValid_ = false;
    int64_t     cachedRangeStart_ = -1;
    int64_t     cachedRangeEnd_   = -1;

    void paintWaveform(juce::Graphics& g);
    void paintGrid(juce::Graphics& g);
    void paintMarkers(juce::Graphics& g);
    void paintPlayhead(juce::Graphics& g);
};
