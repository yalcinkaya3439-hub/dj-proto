#include "WaveformDisplay.h"
#include <cmath>
#include <algorithm>

// ============================================================================
// OverviewWaveform
// ============================================================================

OverviewWaveform::OverviewWaveform() {
    startTimerHz(15);
    setOpaque(true);
}

OverviewWaveform::~OverviewWaveform() { stopTimer(); }

void OverviewWaveform::setWaveformData(const djcore::WaveformPeaks* peaks,
                                       const std::atomic<double>* playheadPtr,
                                       int64_t totalSamples)
{
    peaks_        = peaks;
    playheadPtr_  = playheadPtr;
    totalSamples_ = totalSamples;
    cacheValid_   = false;
    repaint();
}

void OverviewWaveform::clear() {
    peaks_        = nullptr;
    playheadPtr_  = nullptr;
    totalSamples_ = 0;
    cacheValid_   = false;
    repaint();
}

void OverviewWaveform::setLoopRegion(double inSample, double outSample, bool active) {
    loopIn_     = inSample;
    loopOut_    = outSample;
    loopActive_ = active;
    repaint();
}

void OverviewWaveform::setCuePoint(double sample) {
    cuePoint_ = sample;
    repaint();
}

void OverviewWaveform::timerCallback() { repaint(); }

double OverviewWaveform::xToSample(float x) const {
    if (totalSamples_ <= 0 || getWidth() <= 0) return 0.0;
    return static_cast<double>(x) / getWidth() * static_cast<double>(totalSamples_);
}

double OverviewWaveform::sampleToX(double sample) const {
    if (totalSamples_ <= 0 || getWidth() <= 0) return 0.0;
    return sample / static_cast<double>(totalSamples_) * getWidth();
}

void OverviewWaveform::rebuildCache() {
    int w = getWidth();
    int h = getHeight();
    if (w <= 0 || h <= 0 || !peaks_ || totalSamples_ <= 0) return;

    cache_ = juce::Image(juce::Image::RGB, w, h, true);
    juce::Graphics cg(cache_);
    cg.fillAll(juce::Colour(0xff0a0a14));

    auto slicePeaks = djcore::WaveformAnalyzer::getPeaksForRange(
        *peaks_, 0, totalSamples_, w);

    int midY = h / 2;
    cg.setColour(juce::Colour(0xff007c8a));
    for (int x = 0; x < w; ++x) {
        float peak = x < static_cast<int>(slicePeaks.size()) ? slicePeaks[x] : 0.0f;
        int bh = std::max(1, static_cast<int>(peak * midY));
        cg.fillRect(x, midY - bh, 1, bh * 2);
    }
    cacheValid_ = true;
    cachedW_    = w;
}

void OverviewWaveform::paint(juce::Graphics& g) {
    int w = getWidth();
    int h = getHeight();

    if (!peaks_ || totalSamples_ <= 0) {
        g.fillAll(juce::Colour(0xff0a0a14));
        return;
    }

    if (!cacheValid_ || cachedW_ != w) rebuildCache();
    if (cache_.isValid()) g.drawImage(cache_, 0, 0, w, h, 0, 0, w, h);

    // Loop region
    if (loopIn_ < loopOut_) {
        float lx = static_cast<float>(sampleToX(loopIn_));
        float rx = static_cast<float>(sampleToX(loopOut_));
        g.setColour(loopActive_ ? juce::Colour(0x4400e676) : juce::Colour(0x2200e676));
        g.fillRect(lx, 0.0f, rx - lx, static_cast<float>(h));
        g.setColour(juce::Colour(0xff00e676));
        g.drawLine(lx, 0, lx, static_cast<float>(h), 1.0f);
        g.drawLine(rx, 0, rx, static_cast<float>(h), 1.0f);
    }

    // Cue
    {
        float cx = static_cast<float>(sampleToX(cuePoint_));
        g.setColour(juce::Colour(0xff2979ff).withAlpha(0.8f));
        g.drawLine(cx, 0, cx, static_cast<float>(h), 1.0f);
    }

    // Playhead
    if (playheadPtr_) {
        float px = static_cast<float>(sampleToX(playheadPtr_->load()));
        g.setColour(juce::Colours::white);
        g.drawLine(px, 0, px, static_cast<float>(h), 2.0f);
    }
}

void OverviewWaveform::resized() {
    cacheValid_ = false;
}

void OverviewWaveform::mouseDown(const juce::MouseEvent& e) {
    if (!peaks_) return;
    double s = xToSample(static_cast<float>(e.x));
    s = std::max(0.0, std::min(s, static_cast<double>(totalSamples_ - 1)));
    if (onSeek) onSeek(s);
}

void OverviewWaveform::mouseDrag(const juce::MouseEvent& e) {
    mouseDown(e);
}

// ============================================================================
// WaveformDisplay
// ============================================================================

WaveformDisplay::WaveformDisplay() {
    startTimerHz(30);
    setOpaque(true);
}

WaveformDisplay::~WaveformDisplay() { stopTimer(); }

void WaveformDisplay::setWaveformData(const djcore::WaveformPeaks* peaks,
                                      const djcore::BeatGrid*      grid,
                                      const std::atomic<double>*   playheadPtr,
                                      int64_t                      totalSamples)
{
    peaks_        = peaks;
    grid_         = grid;
    playheadPtr_  = playheadPtr;
    totalSamples_ = totalSamples;
    isScrubbing_  = false;
    dragMode_     = DragMode::None;
    repaint();
}

void WaveformDisplay::clear() {
    peaks_        = nullptr;
    grid_         = nullptr;
    playheadPtr_  = nullptr;
    totalSamples_ = 0;
    loopSet_      = false;
    isScrubbing_  = false;
    dragMode_     = DragMode::None;
    repaint();
}

void WaveformDisplay::setLoopRegion(double inSample, double outSample, bool active) {
    loopIn_     = inSample;
    loopOut_    = outSample;
    loopActive_ = active;
    loopSet_    = (inSample < outSample);
    repaint();
}

void WaveformDisplay::setCuePoint(double sample) {
    cuePoint_ = sample;
    repaint();
}

void WaveformDisplay::setHalfWindow(int64_t halfSamples) {
    halfWindowSamples_ = std::max<int64_t>(1024, halfSamples);
    repaint();
}

void WaveformDisplay::zoomIn() {
    setHalfWindow(halfWindowSamples_ / 2);
}

void WaveformDisplay::zoomOut() {
    setHalfWindow(std::min(halfWindowSamples_ * 2,
                           static_cast<int64_t>(totalSamples_ / 2 + 1)));
}

double WaveformDisplay::getDisplayCenter() const {
    if (isScrubbing_) return scrubSample_;
    if (playheadPtr_) return playheadPtr_->load();
    return 0.0;
}

double WaveformDisplay::closupSampleToX(double sample) const {
    double center = getDisplayCenter();
    double visStart = center - static_cast<double>(halfWindowSamples_);
    double range    = static_cast<double>(halfWindowSamples_) * 2.0;
    if (range <= 0.0) return 0.0;
    return (sample - visStart) / range * getWidth();
}

double WaveformDisplay::closupXToSample(float x) const {
    double center = getDisplayCenter();
    double visStart = center - static_cast<double>(halfWindowSamples_);
    double range    = static_cast<double>(halfWindowSamples_) * 2.0;
    return visStart + static_cast<double>(x) / getWidth() * range;
}

bool WaveformDisplay::isNearLoopIn(float x) const {
    if (!loopSet_) return false;
    float inX = static_cast<float>(closupSampleToX(loopIn_));
    return std::abs(x - inX) < 10.0f;
}

bool WaveformDisplay::isNearLoopOut(float x) const {
    if (!loopSet_) return false;
    float outX = static_cast<float>(closupSampleToX(loopOut_));
    return std::abs(x - outX) < 10.0f;
}

void WaveformDisplay::timerCallback() { repaint(); }

juce::String WaveformDisplay::getPositionBarText() const {
    if (!grid_ || totalSamples_ <= 0) return {};
    double ph       = getDisplayCenter();
    double firstBeat = grid_->getFirstBeat();
    double measDur  = grid_->measureDurationSamples();
    double subDur   = grid_->subBeatDurationSamples();
    if (measDur <= 0.0 || subDur <= 0.0) return {};

    double rel = ph - firstBeat;
    int barNum = static_cast<int>(std::floor(rel / measDur)) + 1;
    if (barNum < 1) barNum = 1;

    double posInBar = std::fmod(rel, measDur);
    if (posInBar < 0.0) posInBar += measDur;
    int subBeatNum   = static_cast<int>(std::floor(posInBar / subDur)) + 1;
    int totalSubBeats = grid_->getTimeSig().numerator;

    const auto& ts = grid_->getTimeSig();
    juce::String tsStr = juce::String(ts.numerator) + "/" + juce::String(ts.denominator);

    return "Bar " + juce::String(barNum)
        + " | Beat " + juce::String(subBeatNum) + "/" + juce::String(totalSubBeats)
        + " | " + tsStr;
}

// ---- paint -----------------------------------------------------------------

void WaveformDisplay::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff0d1117));

    if (!peaks_ || totalSamples_ == 0) {
        g.setColour(juce::Colours::grey);
        g.drawText("No file loaded", getLocalBounds(), juce::Justification::centred);
        return;
    }

    paintWaveform(g);
    paintGrid(g);
    paintLoopRegion(g);
    paintCue(g);
    paintPlayhead(g);
    paintPositionBar(g);
}

void WaveformDisplay::paintWaveform(juce::Graphics& g) {
    int w    = getWidth();
    int h    = getHeight();
    int midY = h / 2;

    double center   = getDisplayCenter();
    double visStart = center - static_cast<double>(halfWindowSamples_);
    double visEnd   = center + static_cast<double>(halfWindowSamples_);

    int64_t clampStart = static_cast<int64_t>(std::max(0.0, visStart));
    int64_t clampEnd   = static_cast<int64_t>(std::min(static_cast<double>(totalSamples_), visEnd));

    // Background
    g.setColour(juce::Colour(0xff0d1117));
    g.fillAll();

    if (clampStart >= clampEnd) return;

    auto slicePeaks = djcore::WaveformAnalyzer::getPeaksForRange(
        *peaks_, clampStart, clampEnd, w);

    // Compute pixel offset for clamped start
    double totalRange = visEnd - visStart;
    int pixelOffset = (totalRange > 0.0)
        ? static_cast<int>((static_cast<double>(clampStart) - visStart) / totalRange * w)
        : 0;

    g.setColour(juce::Colour(0xff00bcd4));
    for (int i = 0; i < static_cast<int>(slicePeaks.size()); ++i) {
        int x = pixelOffset + i;
        if (x < 0 || x >= w) continue;
        float peak = slicePeaks[static_cast<size_t>(i)];
        int bh = std::max(1, static_cast<int>(peak * midY));
        g.fillRect(x, midY - bh, 1, bh * 2);
    }
}

void WaveformDisplay::paintGrid(juce::Graphics& g) {
    if (!grid_) return;
    int h = getHeight();

    double center   = getDisplayCenter();
    double visStart = center - static_cast<double>(halfWindowSamples_);
    double visEnd   = center + static_cast<double>(halfWindowSamples_);

    auto marks = grid_->getMarksInRange(visStart, visEnd);
    for (const auto& m : marks) {
        float x = static_cast<float>(closupSampleToX(m.samplePosition));
        if (x < 0 || x > getWidth()) continue;

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

void WaveformDisplay::paintLoopRegion(juce::Graphics& g) {
    if (!loopSet_) return;
    int h = getHeight();
    float lx = static_cast<float>(closupSampleToX(loopIn_));
    float rx = static_cast<float>(closupSampleToX(loopOut_));
    float w  = static_cast<float>(getWidth());

    float drawLx = std::max(0.0f, lx);
    float drawRx = std::min(w,     rx);

    if (drawRx > drawLx) {
        juce::Colour fill = loopActive_
            ? juce::Colour(0x4400e676)
            : juce::Colour(0x2200e676);
        g.setColour(fill);
        g.fillRect(drawLx, 0.0f, drawRx - drawLx, static_cast<float>(h));
    }

    g.setColour(juce::Colour(0xff00e676));
    // IN line + handle
    if (lx >= -2.0f && lx <= w + 2.0f) {
        g.drawLine(lx, 0, lx, static_cast<float>(h), 2.0f);
        juce::Path inHandle;
        inHandle.addTriangle(lx, static_cast<float>(h) - 16,
                             lx + 12, static_cast<float>(h),
                             lx,      static_cast<float>(h));
        g.fillPath(inHandle);

        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText("IN", static_cast<int>(lx) + 3, 3, 24, 12, juce::Justification::left);
    }
    // OUT line + handle
    if (rx >= -2.0f && rx <= w + 2.0f) {
        g.drawLine(rx, 0, rx, static_cast<float>(h), 2.0f);
        juce::Path outHandle;
        outHandle.addTriangle(rx,      static_cast<float>(h) - 16,
                              rx - 12, static_cast<float>(h),
                              rx,      static_cast<float>(h));
        g.fillPath(outHandle);

        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText("OUT", static_cast<int>(rx) - 27, 3, 27, 12, juce::Justification::right);
    }
}

void WaveformDisplay::paintCue(juce::Graphics& g) {
    float cx = static_cast<float>(closupSampleToX(cuePoint_));
    if (cx < 0 || cx > getWidth()) return;
    g.setColour(juce::Colour(0xff2979ff));
    g.drawLine(cx, 0, cx, static_cast<float>(getHeight()), 1.5f);
    juce::Path arrow;
    arrow.addTriangle(cx - 5, 0, cx + 5, 0, cx, 12);
    g.fillPath(arrow);
}

void WaveformDisplay::paintPlayhead(juce::Graphics& g) {
    // Playhead is always at the center pixel
    float phX = getWidth() / 2.0f;
    g.setColour(juce::Colours::white);
    g.drawLine(phX, 0, phX, static_cast<float>(getHeight()), 2.5f);
    // Small diamond at top
    juce::Path diamond;
    diamond.addTriangle(phX - 5, 0, phX + 5, 0, phX, 10);
    g.fillPath(diamond);
}

void WaveformDisplay::paintPositionBar(juce::Graphics& g) {
    auto text = getPositionBarText();
    if (text.isEmpty()) return;
    g.setFont(juce::Font(11.0f));
    g.setColour(juce::Colours::white.withAlpha(0.75f));
    // Draw in top-right so it doesn't clash with IN/OUT labels
    g.drawText(text, 4, getHeight() - 18, getWidth() - 8, 16,
               juce::Justification::right);
}

// ---- Mouse -----------------------------------------------------------------

void WaveformDisplay::mouseDown(const juce::MouseEvent& e) {
    if (!peaks_) return;

    float x = static_cast<float>(e.x);

    if (isNearLoopIn(x)) {
        dragMode_ = DragMode::DraggingLoopIn;
        return;
    }
    if (isNearLoopOut(x)) {
        dragMode_ = DragMode::DraggingLoopOut;
        return;
    }

    // Scrub drag
    dragMode_    = DragMode::Scrubbing;
    dragStartX_  = x;
    dragStartPh_ = getDisplayCenter();
    scrubSample_ = dragStartPh_;
    isScrubbing_ = true;
    if (onScrubStart) onScrubStart();
}

void WaveformDisplay::mouseDrag(const juce::MouseEvent& e) {
    if (!peaks_) return;
    float x = static_cast<float>(e.x);

    if (dragMode_ == DragMode::Scrubbing) {
        double range    = static_cast<double>(halfWindowSamples_) * 2.0;
        double perPixel = (getWidth() > 0) ? range / getWidth() : 1.0;
        // Dragging right = going backwards in track (waveform scrolls left = earlier material)
        double delta = static_cast<double>(dragStartX_ - x) * perPixel;
        scrubSample_ = std::max(0.0,
            std::min(dragStartPh_ + delta,
                     static_cast<double>(totalSamples_ - 1)));
        repaint();
    }
    else if (dragMode_ == DragMode::DraggingLoopIn) {
        double sample = closupXToSample(x);
        sample = std::max(0.0, std::min(sample, loopOut_ - 1.0));
        loopIn_ = sample;
        loopSet_ = (loopIn_ < loopOut_);
        if (onLoopInChanged) onLoopInChanged(sample);
        repaint();
    }
    else if (dragMode_ == DragMode::DraggingLoopOut) {
        double sample = closupXToSample(x);
        sample = std::max(loopIn_ + 1.0, std::min(sample,
                          static_cast<double>(totalSamples_ - 1)));
        loopOut_ = sample;
        loopSet_ = (loopIn_ < loopOut_);
        if (onLoopOutChanged) onLoopOutChanged(sample);
        repaint();
    }
}

void WaveformDisplay::mouseUp(const juce::MouseEvent& /*e*/) {
    if (dragMode_ == DragMode::Scrubbing) {
        isScrubbing_ = false;
        if (onScrubEnd) onScrubEnd(scrubSample_);
    }
    dragMode_ = DragMode::None;
    isScrubbing_ = false;
}

void WaveformDisplay::mouseMove(const juce::MouseEvent& e) {
    float x = static_cast<float>(e.x);
    if (isNearLoopIn(x) || isNearLoopOut(x))
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void WaveformDisplay::mouseWheelMove(const juce::MouseEvent& e,
                                     const juce::MouseWheelDetails& wheel) {
    if (e.mods.isCtrlDown()) {
        if (wheel.deltaY > 0) zoomIn();
        else                  zoomOut();
    }
}

void WaveformDisplay::resized() {}
