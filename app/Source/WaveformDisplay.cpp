#include "WaveformDisplay.h"
#include <cmath>
#include <algorithm>

WaveformDisplay::WaveformDisplay() {
    startTimerHz(30);  // 30 fps repaint
    setOpaque(true);
}

WaveformDisplay::~WaveformDisplay() {
    stopTimer();
}

void WaveformDisplay::setWaveformData(
    const djcore::WaveformPeaks* peaks,
    const djcore::BeatGrid*      grid,
    const std::atomic<double>*   playheadSamplePtr,
    int64_t                      totalSamples)
{
    peaks_        = peaks;
    grid_         = grid;
    playheadPtr_  = playheadSamplePtr;
    totalSamples_ = totalSamples;
    cacheValid_   = false;

    visibleStart_ = 0;
    visibleEnd_   = std::min(totalSamples, defaultWindowSamples_);
    repaint();
}

void WaveformDisplay::clear() {
    peaks_       = nullptr;
    grid_        = nullptr;
    playheadPtr_ = nullptr;
    totalSamples_ = 0;
    cacheValid_  = false;
    repaint();
}

void WaveformDisplay::setVisibleRange(int64_t startSample, int64_t endSample) {
    visibleStart_ = std::max<int64_t>(0, startSample);
    visibleEnd_   = std::min(endSample, totalSamples_);
    cacheValid_   = false;
    repaint();
}

void WaveformDisplay::zoomIn() {
    int64_t center = (visibleStart_ + visibleEnd_) / 2;
    int64_t half   = (visibleEnd_ - visibleStart_) / 4;
    half = std::max<int64_t>(half, 1024);
    setVisibleRange(center - half, center + half);
}

void WaveformDisplay::zoomOut() {
    int64_t center = (visibleStart_ + visibleEnd_) / 2;
    int64_t half   = (visibleEnd_ - visibleStart_);
    half = std::min(half, totalSamples_);
    setVisibleRange(center - half, center + half);
}

void WaveformDisplay::centerOnPlayhead() {
    if (!playheadPtr_) return;
    double ph    = playheadPtr_->load();
    int64_t span = visibleEnd_ - visibleStart_;
    setVisibleRange(static_cast<int64_t>(ph) - span / 2,
                    static_cast<int64_t>(ph) + span / 2);
}

void WaveformDisplay::setLoopRegion(double startSample, double endSample, bool active) {
    loopStartSample_ = startSample;
    loopEndSample_   = endSample;
    loopActive_      = active;
    repaint();
}

void WaveformDisplay::setCuePoint(double sample) {
    cuePoint_ = sample;
    repaint();
}

void WaveformDisplay::timerCallback() {
    repaint();
}

double WaveformDisplay::sampleToX(double sample) const {
    double range = static_cast<double>(visibleEnd_ - visibleStart_);
    if (range <= 0.0) return 0.0;
    return (sample - static_cast<double>(visibleStart_)) / range * getWidth();
}

double WaveformDisplay::xToSample(float x) const {
    double range = static_cast<double>(visibleEnd_ - visibleStart_);
    return static_cast<double>(visibleStart_) + (x / getWidth()) * range;
}

void WaveformDisplay::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff1a1a2e));
    if (!peaks_ || totalSamples_ == 0) {
        g.setColour(juce::Colours::grey);
        g.drawText("No file loaded", getLocalBounds(), juce::Justification::centred);
        return;
    }
    paintWaveform(g);
    if (grid_) paintGrid(g);
    paintMarkers(g);
    paintPlayhead(g);
}

void WaveformDisplay::paintWaveform(juce::Graphics& g) {
    int w = getWidth();
    int h = getHeight();
    int midY = h / 2;

    if (!cacheValid_ ||
        cachedRangeStart_ != visibleStart_ ||
        cachedRangeEnd_   != visibleEnd_   ||
        waveformCache_.getWidth() != w)
    {
        waveformCache_ = juce::Image(juce::Image::RGB, w, h, true);
        juce::Graphics cg(waveformCache_);
        cg.fillAll(juce::Colour(0xff1a1a2e));

        auto slicePeaks = djcore::WaveformAnalyzer::getPeaksForRange(
            *peaks_, visibleStart_, visibleEnd_, w);

        cg.setColour(juce::Colour(0xff00bcd4));
        for (int x = 0; x < w; ++x) {
            float peak = x < static_cast<int>(slicePeaks.size()) ? slicePeaks[x] : 0.0f;
            int barHeight = static_cast<int>(peak * midY);
            cg.fillRect(x, midY - barHeight, 1, barHeight * 2);
        }
        cacheValid_       = true;
        cachedRangeStart_ = visibleStart_;
        cachedRangeEnd_   = visibleEnd_;
    }

    g.drawImage(waveformCache_, 0, 0, w, h, 0, 0, w, h);
}

void WaveformDisplay::paintGrid(juce::Graphics& g) {
    if (!grid_) return;
    int h = getHeight();

    auto marks = grid_->getMarksInRange(
        static_cast<double>(visibleStart_),
        static_cast<double>(visibleEnd_));

    for (const auto& m : marks) {
        float x = static_cast<float>(sampleToX(m.samplePosition));

        switch (m.level) {
            case djcore::BeatLevel::PhraseStart:
                g.setColour(juce::Colour(0xffff9800).withAlpha(0.9f));
                g.drawLine(x, 0, x, static_cast<float>(h), 2.5f);
                break;
            case djcore::BeatLevel::MeasureStart:
                g.setColour(juce::Colour(0xffff5252).withAlpha(0.85f));
                g.drawLine(x, 0, x, static_cast<float>(h), 2.0f);
                break;
            case djcore::BeatLevel::GroupStart:
                g.setColour(juce::Colour(0xffffeb3b).withAlpha(0.7f));
                g.drawLine(x, h / 4, x, 3 * h / 4, 1.5f);
                break;
            case djcore::BeatLevel::SubBeat:
                g.setColour(juce::Colour(0xff9e9e9e).withAlpha(0.4f));
                g.drawLine(x, h / 3, x, 2 * h / 3, 1.0f);
                break;
        }
    }
}

void WaveformDisplay::paintMarkers(juce::Graphics& g) {
    int h = getHeight();

    // Loop region
    if (loopActive_) {
        float lx = static_cast<float>(sampleToX(loopStartSample_));
        float rx = static_cast<float>(sampleToX(loopEndSample_));
        g.setColour(juce::Colour(0x3300e676));
        g.fillRect(lx, 0.0f, rx - lx, static_cast<float>(h));
        g.setColour(juce::Colour(0xff00e676));
        g.drawLine(lx, 0, lx, static_cast<float>(h), 2.0f);
        g.drawLine(rx, 0, rx, static_cast<float>(h), 2.0f);
    }

    // Cue point
    float cx = static_cast<float>(sampleToX(cuePoint_));
    g.setColour(juce::Colour(0xff2979ff));
    g.drawLine(cx, 0, cx, static_cast<float>(h), 2.0f);
    juce::Path arrow;
    arrow.addTriangle(cx, 0, cx + 8, 0, cx, 12);
    g.setColour(juce::Colour(0xff2979ff));
    g.fillPath(arrow);
}

void WaveformDisplay::paintPlayhead(juce::Graphics& g) {
    if (!playheadPtr_) return;
    double ph = playheadPtr_->load();
    if (ph < visibleStart_ || ph > visibleEnd_) return;

    float px = static_cast<float>(sampleToX(ph));
    g.setColour(juce::Colours::white);
    g.drawLine(px, 0, px, static_cast<float>(getHeight()), 2.0f);
}

void WaveformDisplay::resized() {
    cacheValid_ = false;
}

void WaveformDisplay::mouseDown(const juce::MouseEvent& e) {
    if (!peaks_) return;
    double sample = xToSample(static_cast<float>(e.x));
    sample = std::max(0.0, std::min(sample, static_cast<double>(totalSamples_ - 1)));
    if (onSeek) onSeek(sample);
}

void WaveformDisplay::mouseWheelMove(const juce::MouseEvent& /*e*/,
                                     const juce::MouseWheelDetails& wheel) {
    if (wheel.deltaY > 0) zoomIn();
    else                  zoomOut();
}
