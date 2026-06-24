#include "DeckComponent.h"
#include <cmath>
#include <sstream>
#include <iomanip>

static juce::String formatTime(double samples, int sampleRate) {
    if (sampleRate <= 0) return "0:00.0";
    double secs = samples / sampleRate;
    int m = static_cast<int>(secs) / 60;
    double s = secs - m * 60;
    std::ostringstream oss;
    oss << m << ":" << std::setw(4) << std::setfill('0') << std::fixed << std::setprecision(1) << s;
    return juce::String(oss.str());
}

DeckComponent::DeckComponent(AudioDeck& deck, const juce::String& label)
    : deck_(deck), label_(label)
{
    timer_.owner = this;

    deckLabel_.setText(label, juce::dontSendNotification);
    deckLabel_.setFont(juce::Font(20.0f, juce::Font::bold));
    deckLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(deckLabel_);

    for (auto* btn : {&loadButton_, &playButton_, &pauseButton_, &cueButton_, &setCueButton_,
                      &bpmHalfBtn_, &bpmDblBtn_, &bpmDecBtn_, &bpmIncBtn_,
                      &applyGroupingBtn_, &loop1Btn_, &loop2Btn_, &loopOffBtn_,
                      &gridLeftBtn_, &gridRightBtn_}) {
        btn->addListener(this);
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff263238));
        btn->setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
        addAndMakeVisible(btn);
    }

    bpmLabel_.setText("BPM", juce::dontSendNotification);
    bpmLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(bpmLabel_);

    bpmEditor_.setInputRestrictions(8, "0123456789.");
    bpmEditor_.setFont(juce::Font(16.0f));
    bpmEditor_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff37474f));
    bpmEditor_.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    bpmEditor_.onReturnKey = [this] {
        double newBPM = bpmEditor_.getText().getDoubleValue();
        if (newBPM > 0.0) deck_.setBPM(newBPM);
    };
    addAndMakeVisible(bpmEditor_);

    confidenceLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    addAndMakeVisible(confidenceLabel_);

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

    buildGroupingOptions();
    groupingCombo_.addListener(this);
    groupingCombo_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff37474f));
    groupingCombo_.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    addAndMakeVisible(groupingCombo_);

    customGroupingEditor_.setInputRestrictions(20, "0123456789+");
    customGroupingEditor_.setFont(juce::Font(14.0f));
    customGroupingEditor_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff37474f));
    customGroupingEditor_.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    customGroupingEditor_.setTooltip("Custom grouping e.g. 2+2+3");
    addAndMakeVisible(customGroupingEditor_);

    elapsedLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
    remainingLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(elapsedLabel_);
    addAndMakeVisible(remainingLabel_);

    tempoSlider_.setRange(0.5, 2.0, 0.001);
    tempoSlider_.setValue(1.0, juce::dontSendNotification);
    tempoSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    tempoSlider_.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 60, 20);
    tempoSlider_.addListener(this);
    tempoSlider_.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff37474f));
    tempoSlider_.setColour(juce::Slider::thumbColourId, juce::Colour(0xff00bcd4));
    addAndMakeVisible(tempoSlider_);

    tempoLabel_.setText("Speed", juce::dontSendNotification);
    tempoLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(tempoLabel_);

    waveform_.onSeek = [this](double sample) {
        deck_.seekToSample(sample);
    };
    addAndMakeVisible(waveform_);

    timer_.startTimerHz(30);
}

DeckComponent::~DeckComponent() {
    timer_.stopTimer();
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

void DeckComponent::buildGroupingOptions() {
    groupingCombo_.clear();
    // These are populated based on the selected time signature
    groupingCombo_.addItem("Default", 1);
    groupingCombo_.addItem("Custom…", 99);
    groupingCombo_.setSelectedId(1, juce::dontSendNotification);
}

void DeckComponent::applyTimeSig() {
    int tsId = timeSigCombo_.getSelectedId();
    djcore::TimeSignature ts;
    switch (tsId) {
        case 1:  ts = djcore::TimeSignature::make44(); break;
        case 2:  ts = djcore::TimeSignature::make34(); break;
        case 3:  ts = djcore::TimeSignature::make24(); break;
        case 4:  ts = djcore::TimeSignature::make58(); break;
        case 5:  ts = djcore::TimeSignature::make68(); break;
        case 6:  ts = djcore::TimeSignature::make78(); break;
        case 7:  ts = djcore::TimeSignature::make88(); break;
        case 8:  ts = djcore::TimeSignature::make98(); break;
        case 9:  ts = djcore::TimeSignature::make108(); break;
        case 10: ts = djcore::TimeSignature::make128(); break;
        default: ts = djcore::TimeSignature::make44(); break;
    }

    // Apply custom grouping if selected
    int grpId = groupingCombo_.getSelectedId();
    if (grpId == 99) {
        juce::String grpStr = customGroupingEditor_.getText();
        if (grpStr.isNotEmpty()) {
            try {
                auto grp = djcore::TimeSignature::parseGrouping(grpStr.toStdString());
                if (ts.isGroupingValid(grp)) {
                    ts.grouping = grp;
                } else {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon,
                        "Invalid Grouping",
                        "Grouping sum must equal " + juce::String(ts.numerator));
                    return;
                }
            } catch (...) {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Invalid Grouping",
                    "Use format like 2+2+3");
                return;
            }
        }
    } else {
        // Populate preset groupings based on time sig
        if (tsId == 6) {  // 7/8
            std::vector<std::vector<int>> presets = {{2,2,3},{2,3,2},{3,2,2}};
            if (grpId >= 2 && grpId <= 4)
                ts.grouping = presets[grpId - 2];
        } else if (tsId == 8) {  // 9/8
            std::vector<std::vector<int>> presets = {{2,2,2,3},{2,2,3,2},{2,3,2,2},{3,2,2,2}};
            if (grpId >= 2 && grpId <= 5)
                ts.grouping = presets[grpId - 2];
        }
    }

    deck_.setTimeSignature(ts);
    waveform_.setWaveformData(&deck_.waveformPeaks(), &deck_.beatGrid(),
                               &deck_.playbackState().playheadSample,
                               static_cast<int64_t>(deck_.totalSamples()));
}

void DeckComponent::openFile() {
    auto chooser = std::make_shared<juce::FileChooser>(
        "Load Audio File", juce::File{},
        "*.wav;*.mp3;*.flac;*.aiff;*.ogg");

    chooser->launchAsync(juce::FileBrowserComponent::openMode |
                         juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc) {
            auto result = fc.getResult();
            if (!result.exists()) return;
            deck_.loadFile(result, store_, [this] {
                juce::MessageManager::callAsync([this] {
                    waveform_.setWaveformData(
                        &deck_.waveformPeaks(),
                        &deck_.beatGrid(),
                        &deck_.playbackState().playheadSample,
                        static_cast<int64_t>(deck_.totalSamples()));
                    updateUIFromDeck();
                });
            });
        });
}

void DeckComponent::buttonClicked(juce::Button* btn) {
    if (btn == &loadButton_)   { openFile(); return; }
    if (btn == &playButton_)   { deck_.play();  return; }
    if (btn == &pauseButton_)  { deck_.pause(); return; }
    if (btn == &cueButton_)    { deck_.jumpToCue(); return; }
    if (btn == &setCueButton_) { deck_.setCuePoint(deck_.currentSamplePosition()); return; }
    if (btn == &bpmHalfBtn_)   { deck_.setBPMHalf();   updateUIFromDeck(); return; }
    if (btn == &bpmDblBtn_)    { deck_.setBPMDouble(); updateUIFromDeck(); return; }
    if (btn == &bpmDecBtn_)    { deck_.adjustBPM(-0.1); updateUIFromDeck(); return; }
    if (btn == &bpmIncBtn_)    { deck_.adjustBPM(+0.1); updateUIFromDeck(); return; }
    if (btn == &applyGroupingBtn_) { applyTimeSig(); return; }
    if (btn == &loop1Btn_)  { deck_.toggleLoop(1); return; }
    if (btn == &loop2Btn_)  { deck_.toggleLoop(2); return; }
    if (btn == &loopOffBtn_){ deck_.disableLoop(); return; }
    if (btn == &gridLeftBtn_)  { deck_.shiftGrid(-deck_.beatGrid().subBeatDurationSamples() * 0.1); return; }
    if (btn == &gridRightBtn_) { deck_.shiftGrid(+deck_.beatGrid().subBeatDurationSamples() * 0.1); return; }
}

void DeckComponent::comboBoxChanged(juce::ComboBox* cb) {
    if (cb == &timeSigCombo_) {
        // Update grouping presets for the new time sig
        int tsId = timeSigCombo_.getSelectedId();
        groupingCombo_.clear(juce::dontSendNotification);
        groupingCombo_.addItem("Default", 1);
        if (tsId == 6) {  // 7/8
            groupingCombo_.addItem("2+2+3", 2);
            groupingCombo_.addItem("2+3+2", 3);
            groupingCombo_.addItem("3+2+2", 4);
        } else if (tsId == 8) {  // 9/8
            groupingCombo_.addItem("2+2+2+3", 2);
            groupingCombo_.addItem("2+2+3+2", 3);
            groupingCombo_.addItem("2+3+2+2", 4);
            groupingCombo_.addItem("3+2+2+2", 5);
        }
        groupingCombo_.addItem("Custom…", 99);
        groupingCombo_.setSelectedId(1, juce::dontSendNotification);
    }
}

void DeckComponent::sliderValueChanged(juce::Slider* s) {
    if (s == &tempoSlider_)
        deck_.setTempoRatio(tempoSlider_.getValue());
}

void DeckComponent::updateUIFromDeck() {
    if (!deck_.isLoaded()) return;

    double sr    = deck_.sampleRate();
    double pos   = deck_.currentSamplePosition();
    double total = deck_.totalSamples();
    double remaining = total - pos;

    elapsedLabel_.setText("Elapsed: " + formatTime(pos, static_cast<int>(sr)),
                          juce::dontSendNotification);
    remainingLabel_.setText("Remain: -" + formatTime(remaining, static_cast<int>(sr)),
                            juce::dontSendNotification);

    double bpm = deck_.bpm();
    bpmEditor_.setText(juce::String(bpm, 2), juce::dontSendNotification);

    auto conf = deck_.trackAnalysis().confidence;
    juce::String confStr;
    juce::Colour confColour;
    switch (conf) {
        case djcore::BPMConfidence::High:
            confStr = "BPM Conf: High"; confColour = juce::Colours::lightgreen; break;
        case djcore::BPMConfidence::Medium:
            confStr = "BPM Conf: Medium"; confColour = juce::Colours::yellow; break;
        default:
            confStr = "BPM Conf: Low"; confColour = juce::Colours::orangered; break;
    }
    confidenceLabel_.setText(confStr, juce::dontSendNotification);
    confidenceLabel_.setColour(juce::Label::textColourId, confColour);

    // Loop markers
    waveform_.setLoopRegion(
        deck_.playbackState().loopStart.load(),
        deck_.playbackState().loopEnd.load(),
        deck_.playbackState().looping.load());
    waveform_.setCuePoint(deck_.playbackState().cuePoint.load());
}

void DeckComponent::refresh() { updateUIFromDeck(); }

void DeckComponent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff0d1117));
    g.setColour(juce::Colour(0xff30363d));
    g.drawRect(getLocalBounds(), 1);
}

void DeckComponent::resized() {
    auto area = getLocalBounds().reduced(8);
    int rowH = 28;
    int gap  = 4;

    deckLabel_.setBounds(area.removeFromTop(rowH));
    area.removeFromTop(gap);

    // Waveform — take 25% of height
    waveform_.setBounds(area.removeFromTop(area.getHeight() / 4));
    area.removeFromTop(gap);

    // Time display row
    auto timeRow = area.removeFromTop(rowH);
    elapsedLabel_.setBounds(timeRow.removeFromLeft(timeRow.getWidth() / 2));
    remainingLabel_.setBounds(timeRow);
    area.removeFromTop(gap);

    // Transport row
    auto transRow = area.removeFromTop(rowH);
    int btnW = transRow.getWidth() / 5;
    loadButton_.setBounds(transRow.removeFromLeft(btnW));
    playButton_.setBounds(transRow.removeFromLeft(btnW));
    pauseButton_.setBounds(transRow.removeFromLeft(btnW));
    cueButton_.setBounds(transRow.removeFromLeft(btnW));
    setCueButton_.setBounds(transRow);
    area.removeFromTop(gap);

    // Tempo slider
    auto tempoRow = area.removeFromTop(rowH);
    tempoLabel_.setBounds(tempoRow.removeFromLeft(50));
    tempoSlider_.setBounds(tempoRow);
    area.removeFromTop(gap);

    // BPM row
    auto bpmRow = area.removeFromTop(rowH);
    bpmLabel_.setBounds(bpmRow.removeFromLeft(40));
    bpmEditor_.setBounds(bpmRow.removeFromLeft(80));
    bpmHalfBtn_.setBounds(bpmRow.removeFromLeft(30));
    bpmDblBtn_.setBounds(bpmRow.removeFromLeft(30));
    bpmDecBtn_.setBounds(bpmRow.removeFromLeft(30));
    bpmIncBtn_.setBounds(bpmRow.removeFromLeft(30));
    confidenceLabel_.setBounds(bpmRow);
    area.removeFromTop(gap);

    // Time signature row
    auto tsRow = area.removeFromTop(rowH);
    timeSigLabel_.setBounds(tsRow.removeFromLeft(65));
    timeSigCombo_.setBounds(tsRow.removeFromLeft(70));
    tsRow.removeFromLeft(8);
    groupingLabel_.setBounds(tsRow.removeFromLeft(65));
    groupingCombo_.setBounds(tsRow.removeFromLeft(90));
    area.removeFromTop(gap);

    // Custom grouping row
    auto grpRow = area.removeFromTop(rowH);
    customGroupingEditor_.setBounds(grpRow.removeFromLeft(120));
    grpRow.removeFromLeft(4);
    applyGroupingBtn_.setBounds(grpRow.removeFromLeft(60));
    area.removeFromTop(gap);

    // Loop row
    auto loopRow = area.removeFromTop(rowH);
    loop1Btn_.setBounds(loopRow.removeFromLeft(70));
    loop2Btn_.setBounds(loopRow.removeFromLeft(70));
    loopOffBtn_.setBounds(loopRow.removeFromLeft(80));
    area.removeFromTop(gap);

    // Grid nudge row
    auto gridRow = area.removeFromTop(rowH);
    gridNudgeLabel_.setBounds(gridRow.removeFromLeft(80));
    gridLeftBtn_.setBounds(gridRow.removeFromLeft(36));
    gridRightBtn_.setBounds(gridRow.removeFromLeft(36));
}
