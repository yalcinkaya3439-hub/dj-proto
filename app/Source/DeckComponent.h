#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"
#include "WaveformDisplay.h"
#include "djcore/TimeSignature.h"

class DeckComponent : public juce::Component,
                      private juce::Button::Listener,
                      private juce::ComboBox::Listener,
                      private juce::Slider::Listener
{
public:
    explicit DeckComponent(AudioDeck& deck, const juce::String& label);
    ~DeckComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void refresh();

    std::function<void()> onSyncRequest;

private:
    // ---- Listeners ----
    void buttonClicked(juce::Button* btn) override;
    void comboBoxChanged(juce::ComboBox* cb) override;
    void sliderValueChanged(juce::Slider* s) override;

    // ---- Internal helpers ----
    void openFile();
    void setStatus(const juce::String& msg,
                   juce::Colour colour = juce::Colours::lightgrey);
    void updateUIFromDeck();
    void updateLoopInfo();
    void buildTimeSigOptions();
    void updateGroupingOptions(int tsId);
    void applyTimeSig();
    double applySnap(double sample) const;  // applies current snap combo

    // Auto Loop: N measures from current position
    void doAutoLoop(int measures);
    // Set manual loop IN / OUT
    void setLoopIn();
    void setLoopOut();

    // ---- Data ----
    AudioDeck& deck_;
    juce::String label_;

    double manualLoopIn_         = -1.0;
    double manualLoopOut_        = -1.0;
    bool   wasPlayingBeforeScrub_ = false;

    // ---- Waveform area ----
    WaveformDisplay  waveform_;
    OverviewWaveform overview_;

    // ---- Info labels ----
    juce::Label  deckLabel_;
    juce::Label  statusLabel_;
    juce::Label  posBarLabel_;
    juce::Label  elapsedLabel_;
    juce::Label  remainingLabel_;
    juce::Label  loopInfoLabel_;

    // ---- Transport ----
    juce::TextButton loadButton_   {"Load File"};
    juce::TextButton playButton_   {"Play"};
    juce::TextButton pauseButton_  {"Pause"};
    juce::TextButton cueButton_    {"Cue"};
    juce::TextButton setCueButton_ {"Set Cue"};

    // ---- Speed ----
    juce::Label  tempoLabel_;
    juce::Slider tempoSlider_;

    // ---- BPM ----
    juce::Label      bpmLabel_;
    juce::TextEditor bpmEditor_;
    juce::TextButton bpmHalfBtn_ {"/2"};
    juce::TextButton bpmDblBtn_  {"x2"};
    juce::TextButton bpmDecBtn_  {"BPM-"};
    juce::TextButton bpmIncBtn_  {"BPM+"};
    juce::Label      confidenceLabel_;

    // ---- Time Signature ----
    juce::Label      timeSigLabel_;
    juce::ComboBox   timeSigCombo_;
    juce::Label      groupingLabel_;
    juce::ComboBox   groupingCombo_;
    juce::TextEditor customGroupingEditor_;
    juce::TextButton applyGroupingBtn_ {"Apply"};

    // ---- Grid controls ----
    juce::TextButton setGridStartBtn_ {"Set Grid Start"};
    juce::TextButton gridLeftBtn_     {"Grid L"};
    juce::TextButton gridRightBtn_    {"Grid R"};
    juce::TextButton gridFineLeftBtn_ {"Fine L"};
    juce::TextButton gridFineRightBtn_{"Fine R"};
    juce::TextButton zoomInBtn_       {"Zoom In"};
    juce::TextButton zoomOutBtn_      {"Zoom Out"};

    // ---- Auto Loop ----
    juce::TextButton autoLoop1Btn_ {"Auto 1"};
    juce::TextButton autoLoop2Btn_ {"Auto 2"};
    juce::TextButton autoLoop4Btn_ {"Auto 4"};
    juce::TextButton autoLoop8Btn_ {"Auto 8"};

    // ---- Manual Loop ----
    juce::TextButton loopInBtn_    {"Loop In"};
    juce::TextButton loopOutBtn_   {"Loop Out"};
    juce::TextButton loopOnOffBtn_ {"Loop On/Off"};
    juce::TextButton clearLoopBtn_ {"Clear Loop"};
    juce::Label      snapLabel_;
    juce::ComboBox   snapCombo_;     // Off / Sub-beat / Measure

    // ---- File chooser lifetime ----
    std::shared_ptr<juce::FileChooser> activeChooser_;

    // ---- Data store for track analysis cache ----
    djcore::TrackDataStore store_;

    // ---- 30 fps refresh timer ----
    class RefreshTimer : public juce::Timer {
    public:
        DeckComponent* owner;
        void timerCallback() override { owner->updateUIFromDeck(); }
    } timer_;
};
