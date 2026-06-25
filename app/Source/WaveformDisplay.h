#pragma once
#include <JuceHeader.h>
#include "djcore/WaveformAnalyzer.h"
#include "djcore/BeatGrid.h"
#include <atomic>

// ---- OverviewWaveform ------------------------------------------------------
// Thin full-track strip: shows complete waveform, position marker, loop region.
// Click or drag → onSeek callback.
class OverviewWaveform : public juce::Component, private juce::Timer {
public:
    OverviewWaveform();
    ~OverviewWaveform() override;

    void setWaveformData(const djcore::WaveformPeaks* peaks,
                         const std::atomic<double>* playheadPtr,
                         int64_t totalSamples);
    void clear();

    void setLoopRegion(double inSample, double outSample, bool active);
    void setCuePoint(double sample);

    std::function<void(double)> onSeek;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    double xToSample(float x) const;
    double sampleToX(double sample) const;
    void rebuildCache();

    const djcore::WaveformPeaks* peaks_       = nullptr;
    const std::atomic<double>*   playheadPtr_ = nullptr;
    int64_t                      totalSamples_ = 0;

    double loopIn_     = 0.0;
    double loopOut_    = 0.0;
    bool   loopActive_ = false;
    double cuePoint_   = 0.0;

    // Cached peaks for overview width
    juce::Image  cache_;
    bool         cacheValid_     = false;
    int          cachedW_        = 0;
};

// ---- WaveformDisplay -------------------------------------------------------
// Scrolling close waveform: playhead fixed at center, waveform moves.
// Supports drag-scrub, loop-handle drag, zoom.
class WaveformDisplay : public juce::Component, private juce::Timer {
public:
    WaveformDisplay();
    ~WaveformDisplay() override;

    void setWaveformData(const djcore::WaveformPeaks* peaks,
                         const djcore::BeatGrid*      grid,
                         const std::atomic<double>*   playheadPtr,
                         int64_t                      totalSamples);
    void clear();

    void setLoopRegion(double inSample, double outSample, bool active);
    void setCuePoint(double sample);

    // Zoom: number of samples visible on each side of the playhead.
    void setHalfWindow(int64_t halfSamples);
    int64_t getHalfWindow() const { return halfWindowSamples_; }

    void zoomIn();
    void zoomOut();

    // Position bar text: "Bar 12 | Beat 3/9"
    juce::String getPositionBarText() const;

    // Callbacks
    std::function<void()>        onScrubStart;     // drag just started
    std::function<void(double)>  onScrubEnd;       // drag released → commit seek
    std::function<void(double)>  onLoopInChanged;  // IN handle dragged
    std::function<void(double)>  onLoopOutChanged; // OUT handle dragged

    // juce::Component
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove(const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    double getDisplayCenter() const;
    double closupSampleToX(double sample) const;  // relative to component left
    double closupXToSample(float x)       const;

    void paintWaveform(juce::Graphics& g);
    void paintGrid(juce::Graphics& g);
    void paintLoopRegion(juce::Graphics& g);
    void paintCue(juce::Graphics& g);
    void paintPlayhead(juce::Graphics& g);
    void paintPositionBar(juce::Graphics& g);

    bool isNearLoopIn(float x)  const;
    bool isNearLoopOut(float x) const;

    // Data
    const djcore::WaveformPeaks* peaks_        = nullptr;
    const djcore::BeatGrid*      grid_         = nullptr;
    const std::atomic<double>*   playheadPtr_  = nullptr;
    int64_t                      totalSamples_ = 0;

    int64_t halfWindowSamples_ = static_cast<int64_t>(44100 * 3); // 3 s each side

    // Loop
    double loopIn_     = 0.0;
    double loopOut_    = 0.0;
    bool   loopActive_ = false;
    bool   loopSet_    = false;  // IN and OUT both set

    // Cue
    double cuePoint_ = 0.0;

    // Drag state
    enum class DragMode { None, Scrubbing, DraggingLoopIn, DraggingLoopOut };
    DragMode dragMode_    = DragMode::None;
    float    dragStartX_  = 0.0f;
    double   dragStartPh_ = 0.0;
    double   scrubSample_ = 0.0;
    bool     isScrubbing_ = false;
};
