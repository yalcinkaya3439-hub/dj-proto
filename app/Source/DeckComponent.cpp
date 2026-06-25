#include "DeckComponent.h"
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#include <sstream>
#include <iomanip>

// ---- helpers ---------------------------------------------------------------

static juce::String secToMmSs(double secs) {
    if (secs < 0.0) secs = 0.0;
    int m   = static_cast<int>(secs) / 60;
    double s = secs - m * 60.0;
    std::ostringstream oss;
    oss << m << ":" << std::setw(5) << std::setfill('0')
        << std::fixed << std::setprecision(2) << s;
    return juce::String(oss.str());
}

static juce::String samplesToTime(double samples, int sr) {
    if (sr <= 0) return "0:00.00";
    return secToMmSs(samples / sr);
}

static void styleBtn(juce::TextButton& b,
                     juce::Colour bg   = juce::Colour(0xff263238),
                     juce::Colour fg   = juce::Colours::lightgrey) {
    b.setColour(juce::TextButton::buttonColourId, bg);
    b.setColour(juce::TextButton::textColourOffId, fg);
}

// ============================================================================
// DeckComponent
// ============================================================================

DeckComponent::DeckComponent(AudioDeck& deck, const juce::String& label)
    : deck_(deck), label_(label)
{
    timer_.owner = this;

    // ---- Deck label ----
    deckLabel_.setText(label, juce::dontSendNotification);
    deckLabel_.setFont(juce::Font(18.0f, juce::Font::bold));
    deckLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(deckLabel_);

    // ---- Status / file info ----
    statusLabel_.setText("No file loaded", juce::dontSendNotification);
    statusLabel_.setColour(juce::Label::textColourId, juce::Colours::grey);
    statusLabel_.setFont(juce::Font(12.0f));
    addAndMakeVisible(statusLabel_);

    // ---- Position bar ----
    posBarLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    posBarLabel_.setFont(juce::Font(11.0f));
    addAndMakeVisible(posBarLabel_);

    // ---- Time ----
    elapsedLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
    remainingLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
    elapsedLabel_.setFont(juce::Font(12.0f));
    remainingLabel_.setFont(juce::Font(12.0f));
    addAndMakeVisible(elapsedLabel_);
    addAndMakeVisible(remainingLabel_);

    // ---- Waveform ----
    waveform_.onScrubStart = [this] {
        wasPlayingBeforeScrub_ = deck_.isPlaying();
        deck_.pause();
    };
    waveform_.onScrubEnd = [this](double sample) {
        deck_.seekToSample(sample);
        if (wasPlayingBeforeScrub_) deck_.play();
    };
    waveform_.onLoopInChanged = [this](double sample) {
        manualLoopIn_ = sample;
        deck_.setManualLoop(manualLoopIn_, manualLoopOut_);
        updateLoopInfo();
    };
    waveform_.onLoopOutChanged = [this](double sample) {
        manualLoopOut_ = sample;
        deck_.setManualLoop(manualLoopIn_, manualLoopOut_);
        updateLoopInfo();
    };
    addAndMakeVisible(waveform_);

    // ---- Overview ----
    overview_.onSeek = [this](double sample) {
        deck_.seekToSample(sample);
    };
    addAndMakeVisible(overview_);

    // ---- Transport ----
    for (auto* b : {&loadButton_, &playButton_, &pauseButton_, &cueButton_, &setCueButton_}) {
        styleBtn(*b);
        b->addListener(this);
        addAndMakeVisible(b);
    }

    // ---- Speed ----
    tempoLabel_.setText("Speed", juce::dontSendNotification);
    tempoLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(tempoLabel_);

    tempoSlider_.setRange(0.5, 2.0, 0.001);
    tempoSlider_.setValue(1.0, juce::dontSendNotification);
    tempoSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    tempoSlider_.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 55, 20);
    tempoSlider_.addListener(this);
    tempoSlider_.setColour(juce::Slider::thumbColourId, juce::Colour(0xff00bcd4));
    addAndMakeVisible(tempoSlider_);

    // ---- BPM ----
    bpmLabel_.setText("BPM", juce::dontSendNotification);
    bpmLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(bpmLabel_);

    bpmEditor_.setInputRestrictions(8, "0123456789.");
    bpmEditor_.setFont(juce::Font(14.0f));
    bpmEditor_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff37474f));
    bpmEditor_.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    bpmEditor_.onReturnKey = [this] {
        double v = bpmEditor_.getText().getDoubleValue();
        if (v > 0.0) deck_.setBPM(v);
    };
    addAndMakeVisible(bpmEditor_);

    confidenceLabel_.setFont(juce::Font(11.0f));
    confidenceLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    addAndMakeVisible(confidenceLabel_);

    for (auto* b : {&bpmHalfBtn_, &bpmDblBtn_, &bpmDecBtn_, &bpmIncBtn_}) {
        styleBtn(*b);
        b->addListener(this);
        addAndMakeVisible(b);
    }

    // ---- Time sig / grouping ----
    timeSigLabel_.setText("Time Sig", juce::dontSendNotification);
    timeSigLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(timeSigLabel_);

    buildTimeSigOptions();
    timeSigCombo_.addListener(this);
    timeSigCombo_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff37474f));
    timeSigCombo_.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    addAndMakeVisible(timeSigCombo_);

    groupingLabel_.setText("Grouping", juce::dontSendNotification);
    groupingLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(groupingLabel_);

    updateGroupingOptions(1);
    groupingCombo_.addListener(this);
    groupingCombo_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff37474f));
    groupingCombo_.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    addAndMakeVisible(groupingCombo_);

    customGroupingEditor_.setInputRestrictions(20, "0123456789+");
    customGroupingEditor_.setFont(juce::Font(13.0f));
    customGroupingEditor_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff37474f));
    customGroupingEditor_.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    customGroupingEditor_.setTooltip("e.g. 2+2+3");
    addAndMakeVisible(customGroupingEditor_);

    styleBtn(applyGroupingBtn_);
    applyGroupingBtn_.addListener(this);
    addAndMakeVisible(applyGroupingBtn_);

    // ---- Grid controls ----
    for (auto* b : {&setGridStartBtn_, &gridLeftBtn_, &gridRightBtn_,
                    &gridFineLeftBtn_, &gridFineRightBtn_,
                    &zoomInBtn_, &zoomOutBtn_}) {
        styleBtn(*b);
        b->addListener(this);
        addAndMakeVisible(b);
    }

    // ---- Auto Loop ----
    for (auto* b : {&autoLoop1Btn_, &autoLoop2Btn_, &autoLoop4Btn_, &autoLoop8Btn_}) {
        styleBtn(*b, juce::Colour(0xff1a3a2a));
        b->addListener(this);
        addAndMakeVisible(b);
    }

    // ---- Manual Loop ----
    for (auto* b : {&loopInBtn_, &loopOutBtn_, &loopOnOffBtn_, &clearLoopBtn_}) {
        styleBtn(*b, juce::Colour(0xff1a2a3a));
        b->addListener(this);
        addAndMakeVisible(b);
    }

    snapLabel_.setText("Snap:", juce::dontSendNotification);
    snapLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(snapLabel_);

    snapCombo_.addItem("Off",      1);
    snapCombo_.addItem("Sub-beat", 2);
    snapCombo_.addItem("Measure",  3);
    snapCombo_.setSelectedId(1, juce::dontSendNotification);
    snapCombo_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff37474f));
    snapCombo_.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    addAndMakeVisible(snapCombo_);

    // ---- Loop info label ----
    loopInfoLabel_.setFont(juce::Font(11.0f));
    loopInfoLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    loopInfoLabel_.setJustificationType(juce::Justification::topLeft);
    loopInfoLabel_.setText("No loop set", juce::dontSendNotification);
    addAndMakeVisible(loopInfoLabel_);

    timer_.startTimerHz(30);
}

DeckComponent::~DeckComponent() { timer_.stopTimer(); }

// ---- helpers ---------------------------------------------------------------

void DeckComponent::setStatus(const juce::String& msg, juce::Colour colour) {
    statusLabel_.setText(msg, juce::dontSendNotification);
    statusLabel_.setColour(juce::Label::textColourId, colour);
}

void DeckComponent::buildTimeSigOptions() {
    timeSigCombo_.clear();
    timeSigCombo_.addItem("4/4",  1);
    timeSigCombo_.addItem("3/4",  2);
    timeSigCombo_.addItem("2/4",  3);
    timeSigCombo_.addItem("5/8",  4);
    timeSigCombo_.addItem("6/8",  5);
    timeSigCombo_.addItem("7/8",  6);
    timeSigCombo_.addItem("8/8",  7);
    timeSigCombo_.addItem("9/8",  8);
    timeSigCombo_.addItem("10/8", 9);
    timeSigCombo_.addItem("12/8", 10);
    timeSigCombo_.setSelectedId(1, juce::dontSendNotification);
}

void DeckComponent::updateGroupingOptions(int tsId) {
    groupingCombo_.clear(juce::dontSendNotification);

    if (tsId == 6) {  // 7/8 — grouping is mandatory
        groupingCombo_.addItem("2+2+3", 1);
        groupingCombo_.addItem("2+3+2", 2);
        groupingCombo_.addItem("3+2+2", 3);
        groupingCombo_.addItem("Custom...", 99);
        groupingCombo_.setSelectedId(1, juce::dontSendNotification);
    } else if (tsId == 8) {  // 9/8 — grouping is mandatory
        groupingCombo_.addItem("2+2+2+3", 1);
        groupingCombo_.addItem("2+2+3+2", 2);
        groupingCombo_.addItem("2+3+2+2", 3);
        groupingCombo_.addItem("3+2+2+2", 4);
        groupingCombo_.addItem("Custom...", 99);
        groupingCombo_.setSelectedId(1, juce::dontSendNotification);
    } else {
        groupingCombo_.addItem("Default", 1);
        groupingCombo_.addItem("Custom...", 99);
        groupingCombo_.setSelectedId(1, juce::dontSendNotification);
    }
}

double DeckComponent::applySnap(double sample) const {
    int snapMode = snapCombo_.getSelectedId();
    if (snapMode <= 1 || !deck_.isLoaded()) return sample;

    const auto& grid = deck_.beatGrid();
    double firstBeat = grid.getFirstBeat();
    double rel       = sample - firstBeat;
    double unit      = 0.0;

    if (snapMode == 2) unit = grid.subBeatDurationSamples();
    else if (snapMode == 3) unit = grid.measureDurationSamples();

    if (unit <= 0.0) return sample;
    double snapped = firstBeat + std::round(rel / unit) * unit;
    return std::max(0.0, snapped);
}

void DeckComponent::applyTimeSig() {
    int tsId  = timeSigCombo_.getSelectedId();
    int grpId = groupingCombo_.getSelectedId();

    djcore::TimeSignature ts;
    switch (tsId) {
        case 1:  ts = djcore::TimeSignature::make44();  break;
        case 2:  ts = djcore::TimeSignature::make34();  break;
        case 3:  ts = djcore::TimeSignature::make24();  break;
        case 4:  ts = djcore::TimeSignature::make58();  break;
        case 5:  ts = djcore::TimeSignature::make68();  break;
        case 6:  ts = djcore::TimeSignature::make78();  break;
        case 7:  ts = djcore::TimeSignature::make88();  break;
        case 8:  ts = djcore::TimeSignature::make98();  break;
        case 9:  ts = djcore::TimeSignature::make108(); break;
        case 10: ts = djcore::TimeSignature::make128(); break;
        default: ts = djcore::TimeSignature::make44();  break;
    }

    if (grpId == 99) {
        // Custom
        auto txt = customGroupingEditor_.getText();
        if (txt.isNotEmpty()) {
            try {
                auto grp = djcore::TimeSignature::parseGrouping(txt.toStdString());
                if (ts.isGroupingValid(grp))
                    ts.grouping = grp;
                else {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon, "Invalid Grouping",
                        "Sum must equal " + juce::String(ts.numerator));
                    return;
                }
            } catch (...) {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon, "Invalid Grouping", "Use e.g. 2+2+3");
                return;
            }
        }
    } else if (tsId == 6) {  // 7/8 presets
        std::vector<std::vector<int>> presets = {{2,2,3},{2,3,2},{3,2,2}};
        int idx = grpId - 1;
        if (idx >= 0 && idx < static_cast<int>(presets.size()))
            ts.grouping = presets[static_cast<size_t>(idx)];
    } else if (tsId == 8) {  // 9/8 presets
        std::vector<std::vector<int>> presets = {{2,2,2,3},{2,2,3,2},{2,3,2,2},{3,2,2,2}};
        int idx = grpId - 1;
        if (idx >= 0 && idx < static_cast<int>(presets.size()))
            ts.grouping = presets[static_cast<size_t>(idx)];
    }

    deck_.setTimeSignature(ts);
    waveform_.setWaveformData(&deck_.waveformPeaks(), &deck_.beatGrid(),
                              &deck_.playbackState().playheadSample,
                              static_cast<int64_t>(deck_.totalSamples()));
}

void DeckComponent::doAutoLoop(int measures) {
    deck_.toggleLoop(measures);
    bool loopOn = deck_.isLooping();
    if (loopOn) {
        // Reflect auto-loop region in waveform
        double ls = deck_.playbackState().loopStart.load();
        double le = deck_.playbackState().loopEnd.load();
        waveform_.setLoopRegion(ls, le, true);
        overview_.setLoopRegion(ls, le, true);
        // Sync manual-loop tracking
        manualLoopIn_  = ls;
        manualLoopOut_ = le;
    } else {
        waveform_.setLoopRegion(0, 0, false);
        overview_.setLoopRegion(0, 0, false);
    }
    updateLoopInfo();
}

void DeckComponent::setLoopIn() {
    double sample = deck_.currentSamplePosition();
    sample = applySnap(sample);
    manualLoopIn_ = sample;

    if (manualLoopIn_ < manualLoopOut_) {
        deck_.setManualLoop(manualLoopIn_, manualLoopOut_);
        waveform_.setLoopRegion(manualLoopIn_, manualLoopOut_, deck_.isLooping());
        overview_.setLoopRegion(manualLoopIn_, manualLoopOut_, deck_.isLooping());
    } else {
        waveform_.setLoopRegion(manualLoopIn_, manualLoopIn_, false);
    }
    updateLoopInfo();
}

void DeckComponent::setLoopOut() {
    double sample = deck_.currentSamplePosition();
    sample = applySnap(sample);
    manualLoopOut_ = sample;

    if (manualLoopIn_ >= 0.0 && manualLoopIn_ < manualLoopOut_) {
        deck_.setManualLoop(manualLoopIn_, manualLoopOut_);
        waveform_.setLoopRegion(manualLoopIn_, manualLoopOut_, deck_.isLooping());
        overview_.setLoopRegion(manualLoopIn_, manualLoopOut_, deck_.isLooping());
        // Auto-activate loop when OUT is set
        if (!deck_.isLooping()) {
            deck_.enableLoop();
        }
    } else {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon, "Loop Error",
            "Loop Out must be after Loop In.\nSet Loop In first.");
    }
    updateLoopInfo();
}

void DeckComponent::updateLoopInfo() {
    bool loopOn  = deck_.isLooping();
    bool hasLoop = (manualLoopIn_ >= 0.0 && manualLoopIn_ < manualLoopOut_);

    if (!hasLoop && !loopOn) {
        loopInfoLabel_.setText("No loop set", juce::dontSendNotification);
        loopInfoLabel_.setColour(juce::Label::textColourId, juce::Colours::grey);
        return;
    }

    double inS  = deck_.playbackState().loopStart.load();
    double outS = deck_.playbackState().loopEnd.load();
    double sr   = deck_.sampleRate() > 0 ? deck_.sampleRate() : 44100;

    double inSec  = inS  / sr;
    double outSec = outS / sr;
    double durSec = outSec - inSec;

    juce::String measStr = "?";
    if (deck_.isLoaded()) {
        double measDur = deck_.beatGrid().measureDurationSamples();
        if (measDur > 0.0) {
            double measCount = (outS - inS) / measDur;
            measStr = juce::String(measCount, 2) + " bars";
            double deviation = std::abs(measCount - std::round(measCount));
            if (deviation > 0.03)
                measStr += " (not on bar boundary)";
        }
    }

    juce::String info;
    info << (loopOn ? "ACTIVE " : "Set ")
         << (hasLoop ? "Manual Loop" : "Auto Loop") << "\n"
         << "IN:  " << secToMmSs(inSec)  << "\n"
         << "OUT: " << secToMmSs(outSec) << "\n"
         << "Dur: " << juce::String(durSec, 3) << " s  |  " << measStr;

    loopInfoLabel_.setText(info, juce::dontSendNotification);
    loopInfoLabel_.setColour(juce::Label::textColourId,
                             loopOn ? juce::Colour(0xff00e676) : juce::Colours::lightgrey);
}

// ---- openFile --------------------------------------------------------------

void DeckComponent::openFile() {
    activeChooser_ = std::make_shared<juce::FileChooser>(
        "Load Audio File", juce::File{}, "*.wav;*.aiff;*.flac");

    setStatus("Opening file dialog...", juce::Colours::lightgrey);

    activeChooser_->launchAsync(
        juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc) {
            auto result = fc.getResult();
            if (!result.existsAsFile()) {
                setStatus("No file loaded", juce::Colours::grey);
                activeChooser_.reset();
                return;
            }
            setStatus("Loading: " + result.getFileName() + " ...",
                      juce::Colour(0xffffcc00));

            deck_.loadFile(result, store_, [this](LoadResult res) {
                activeChooser_.reset();
                if (!res.ok) {
                    setStatus("Error: " + res.error, juce::Colours::orangered);
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon, "Load Error", res.error);
                    return;
                }

                // Success
                setStatus(
                    res.fileName + "  |  " +
                    juce::String(res.bpm, 1) + " BPM  |  " +
                    secToMmSs(res.durationSec) + "  |  " +
                    juce::String(res.sampleRate / 1000) + " kHz",
                    juce::Colours::lightgreen);

                waveform_.setWaveformData(
                    &deck_.waveformPeaks(), &deck_.beatGrid(),
                    &deck_.playbackState().playheadSample,
                    static_cast<int64_t>(deck_.totalSamples()));

                overview_.setWaveformData(
                    &deck_.waveformPeaks(),
                    &deck_.playbackState().playheadSample,
                    static_cast<int64_t>(deck_.totalSamples()));

                // Reset loop state
                manualLoopIn_  = -1.0;
                manualLoopOut_ = -1.0;
                waveform_.setLoopRegion(0, 0, false);
                overview_.setLoopRegion(0, 0, false);
                updateLoopInfo();
                updateUIFromDeck();
            });
        });
}

// ---- Listeners -------------------------------------------------------------

void DeckComponent::buttonClicked(juce::Button* btn) {
    // Transport
    if (btn == &loadButton_)   { openFile();   return; }
    if (btn == &playButton_)   { deck_.play(); return; }
    if (btn == &pauseButton_)  { deck_.pause(); return; }
    if (btn == &cueButton_)    { deck_.jumpToCue(); return; }
    if (btn == &setCueButton_) {
        deck_.setCuePoint(deck_.currentSamplePosition());
        waveform_.setCuePoint(deck_.currentSamplePosition());
        overview_.setCuePoint(deck_.currentSamplePosition());
        return;
    }

    // BPM
    if (btn == &bpmHalfBtn_) { deck_.setBPMHalf();    updateUIFromDeck(); return; }
    if (btn == &bpmDblBtn_)  { deck_.setBPMDouble();  updateUIFromDeck(); return; }
    if (btn == &bpmDecBtn_)  { deck_.adjustBPM(-0.1); updateUIFromDeck(); return; }
    if (btn == &bpmIncBtn_)  { deck_.adjustBPM(+0.1); updateUIFromDeck(); return; }

    // Time sig apply
    if (btn == &applyGroupingBtn_) { applyTimeSig(); return; }

    // Grid controls
    if (btn == &setGridStartBtn_) {
        // Set first beat to current playhead position
        double pos = deck_.currentSamplePosition();
        deck_.shiftGrid(pos - deck_.beatGrid().getFirstBeat());
        return;
    }
    // Normal grid nudge: 10 ms
    if (btn == &gridLeftBtn_) {
        double ms10 = deck_.sampleRate() * 0.010;
        deck_.shiftGrid(-ms10);
        return;
    }
    if (btn == &gridRightBtn_) {
        double ms10 = deck_.sampleRate() * 0.010;
        deck_.shiftGrid(+ms10);
        return;
    }
    // Fine nudge: 1 ms
    if (btn == &gridFineLeftBtn_) {
        double ms1 = deck_.sampleRate() * 0.001;
        deck_.shiftGrid(-ms1);
        return;
    }
    if (btn == &gridFineRightBtn_) {
        double ms1 = deck_.sampleRate() * 0.001;
        deck_.shiftGrid(+ms1);
        return;
    }

    // Zoom
    if (btn == &zoomInBtn_)  { waveform_.zoomIn();  return; }
    if (btn == &zoomOutBtn_) { waveform_.zoomOut(); return; }

    // Auto Loop
    if (btn == &autoLoop1Btn_) { doAutoLoop(1); return; }
    if (btn == &autoLoop2Btn_) { doAutoLoop(2); return; }
    if (btn == &autoLoop4Btn_) { doAutoLoop(4); return; }
    if (btn == &autoLoop8Btn_) { doAutoLoop(8); return; }

    // Manual Loop
    if (btn == &loopInBtn_)  { setLoopIn();  return; }
    if (btn == &loopOutBtn_) { setLoopOut(); return; }
    if (btn == &loopOnOffBtn_) {
        if (deck_.isLooping()) deck_.disableLoop();
        else                   deck_.enableLoop();
        bool on = deck_.isLooping();
        waveform_.setLoopRegion(
            deck_.playbackState().loopStart.load(),
            deck_.playbackState().loopEnd.load(), on);
        overview_.setLoopRegion(
            deck_.playbackState().loopStart.load(),
            deck_.playbackState().loopEnd.load(), on);
        updateLoopInfo();
        return;
    }
    if (btn == &clearLoopBtn_) {
        deck_.disableLoop();
        manualLoopIn_  = -1.0;
        manualLoopOut_ = -1.0;
        waveform_.setLoopRegion(0, 0, false);
        overview_.setLoopRegion(0, 0, false);
        updateLoopInfo();
        return;
    }
}

void DeckComponent::comboBoxChanged(juce::ComboBox* cb) {
    if (cb == &timeSigCombo_)
        updateGroupingOptions(timeSigCombo_.getSelectedId());
}

void DeckComponent::sliderValueChanged(juce::Slider* s) {
    if (s == &tempoSlider_)
        deck_.setTempoRatio(tempoSlider_.getValue());
}

// ---- updateUIFromDeck (30 fps) ---------------------------------------------

void DeckComponent::updateUIFromDeck() {
    // Position bar (always update if loaded)
    posBarLabel_.setText(waveform_.getPositionBarText(), juce::dontSendNotification);

    if (!deck_.isLoaded()) return;

    int    sr    = deck_.sampleRate();
    double pos   = deck_.currentSamplePosition();
    double total = deck_.totalSamples();

    elapsedLabel_.setText("+" + samplesToTime(pos, sr), juce::dontSendNotification);
    remainingLabel_.setText("-" + samplesToTime(total - pos, sr), juce::dontSendNotification);

    bpmEditor_.setText(juce::String(deck_.bpm(), 2), juce::dontSendNotification);

    auto conf = deck_.trackAnalysis().confidence;
    juce::String confStr;
    juce::Colour confCol;
    switch (conf) {
        case djcore::BPMConfidence::High:
            confStr = "BPM: High";   confCol = juce::Colours::lightgreen; break;
        case djcore::BPMConfidence::Medium:
            confStr = "BPM: Med";    confCol = juce::Colours::yellow;     break;
        default:
            confStr = "BPM: Low";   confCol = juce::Colours::orangered;  break;
    }
    confidenceLabel_.setText(confStr, juce::dontSendNotification);
    confidenceLabel_.setColour(juce::Label::textColourId, confCol);

    // Sync loop state to waveform display
    bool loopOn = deck_.isLooping();
    double ls = deck_.playbackState().loopStart.load();
    double le = deck_.playbackState().loopEnd.load();
    if (ls < le) {
        waveform_.setLoopRegion(ls, le, loopOn);
        overview_.setLoopRegion(ls, le, loopOn);
    }

    // Cue
    waveform_.setCuePoint(deck_.playbackState().cuePoint.load());
    overview_.setCuePoint(deck_.playbackState().cuePoint.load());
}

void DeckComponent::refresh() { updateUIFromDeck(); }

// ---- paint / resized -------------------------------------------------------

void DeckComponent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff0d1117));
    g.setColour(juce::Colour(0xff30363d));
    g.drawRect(getLocalBounds(), 1);
}

void DeckComponent::resized() {
    auto area = getLocalBounds().reduced(6);
    const int rowH = 24;
    const int gap  = 3;
    auto takeRow   = [&]{ return area.removeFromTop(rowH); };
    auto takeGap   = [&]{ area.removeFromTop(gap); };

    // Deck label
    deckLabel_.setBounds(takeRow());
    takeGap();

    // Waveform — 38% of remaining height (close + overview together)
    int waveH = (area.getHeight() * 38) / 100;
    int closeH   = (waveH * 77) / 100;
    int overviewH = waveH - closeH;
    waveform_.setBounds(area.removeFromTop(closeH));
    overview_.setBounds(area.removeFromTop(overviewH));
    takeGap();

    // Position bar
    posBarLabel_.setBounds(takeRow());
    takeGap();

    // Status
    statusLabel_.setBounds(takeRow());
    takeGap();

    // Time row
    auto timeRow = takeRow();
    elapsedLabel_.setBounds(timeRow.removeFromLeft(timeRow.getWidth() / 2));
    remainingLabel_.setBounds(timeRow);
    takeGap();

    // Transport
    auto transRow = takeRow();
    int btnW = transRow.getWidth() / 5;
    loadButton_.setBounds(transRow.removeFromLeft(btnW));
    playButton_.setBounds(transRow.removeFromLeft(btnW));
    pauseButton_.setBounds(transRow.removeFromLeft(btnW));
    cueButton_.setBounds(transRow.removeFromLeft(btnW));
    setCueButton_.setBounds(transRow);
    takeGap();

    // Speed
    auto speedRow = takeRow();
    tempoLabel_.setBounds(speedRow.removeFromLeft(46));
    tempoSlider_.setBounds(speedRow);
    takeGap();

    // BPM row
    auto bpmRow = takeRow();
    bpmLabel_.setBounds(bpmRow.removeFromLeft(34));
    bpmEditor_.setBounds(bpmRow.removeFromLeft(70));
    bpmHalfBtn_.setBounds(bpmRow.removeFromLeft(28));
    bpmDblBtn_.setBounds(bpmRow.removeFromLeft(28));
    bpmDecBtn_.setBounds(bpmRow.removeFromLeft(40));
    bpmIncBtn_.setBounds(bpmRow.removeFromLeft(40));
    confidenceLabel_.setBounds(bpmRow);
    takeGap();

    // Time sig + grouping row
    auto tsRow = takeRow();
    timeSigLabel_.setBounds(tsRow.removeFromLeft(58));
    timeSigCombo_.setBounds(tsRow.removeFromLeft(60));
    tsRow.removeFromLeft(4);
    groupingLabel_.setBounds(tsRow.removeFromLeft(58));
    groupingCombo_.setBounds(tsRow.removeFromLeft(80));
    tsRow.removeFromLeft(4);
    customGroupingEditor_.setBounds(tsRow.removeFromLeft(60));
    applyGroupingBtn_.setBounds(tsRow.removeFromLeft(48));
    takeGap();

    // Grid row
    auto gridRow = takeRow();
    setGridStartBtn_.setBounds(gridRow.removeFromLeft(90));
    gridRow.removeFromLeft(4);
    gridLeftBtn_.setBounds(gridRow.removeFromLeft(48));
    gridFineLeftBtn_.setBounds(gridRow.removeFromLeft(44));
    gridFineRightBtn_.setBounds(gridRow.removeFromLeft(44));
    gridRightBtn_.setBounds(gridRow.removeFromLeft(48));
    gridRow.removeFromLeft(6);
    zoomInBtn_.setBounds(gridRow.removeFromLeft(54));
    zoomOutBtn_.setBounds(gridRow.removeFromLeft(54));
    takeGap();

    // Auto loop row
    auto alRow = takeRow();
    {
        int w4 = alRow.getWidth() / 4;
        autoLoop1Btn_.setBounds(alRow.removeFromLeft(w4));
        autoLoop2Btn_.setBounds(alRow.removeFromLeft(w4));
        autoLoop4Btn_.setBounds(alRow.removeFromLeft(w4));
        autoLoop8Btn_.setBounds(alRow);
    }
    takeGap();

    // Manual loop row
    auto mlRow = takeRow();
    loopInBtn_.setBounds(mlRow.removeFromLeft(56));
    loopOutBtn_.setBounds(mlRow.removeFromLeft(56));
    loopOnOffBtn_.setBounds(mlRow.removeFromLeft(76));
    clearLoopBtn_.setBounds(mlRow.removeFromLeft(70));
    mlRow.removeFromLeft(4);
    snapLabel_.setBounds(mlRow.removeFromLeft(38));
    snapCombo_.setBounds(mlRow.removeFromLeft(72));
    takeGap();

    // Loop info — remaining space
    loopInfoLabel_.setBounds(area);
}
