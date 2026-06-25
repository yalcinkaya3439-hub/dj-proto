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
    void buttonClicked(juce::Button* btn) override;
    void comboBoxChanged(juce::ComboBox* cb) override;
    void sliderValueChanged(juce::Slider* s) override;

    void openFile();
    void updateUIFromDeck();
    void buildTimeSigOptions();
    void buildGroupingOptions();
    void applyTimeSig();
    void setStatus(const juce::String& msg, juce::Colour colour = juce::Colours::lightgrey);

    AudioDeck& deck_;
    juce::String label_;

    // ---- Widgets ----
    juce::Label          deckLabel_;
    juce::TextButton     loadButton_   {"Load File"};
    juce::TextButton     playButton_   {"Play"};
    juce::TextButton     pauseButton_  {"Pause"};
    juce::TextButton     cueButton_    {"Cue"};
    juce::TextButton     setCueButton_ {"Set Cue"};

    // Status / filename display
    juce::Label          statusLabel_;

    // BPM section
    juce::Label          bpmLabel_;
    juce::TextEditor     bpmEditor_;
    juce::TextButton     bpmHalfBtn_   {"/2"};
    juce::TextButton     bpmDblBtn_    {"x2"};
    juce::TextButton     bpmDecBtn_    {"..."};
    juce::TextButton     bpmIncBtn_    {"+"};
    juce::Label          confidenceLabel_;

    // Time signature
    juce::Label          timeSigLabel_;
    juce::ComboBox       timeSigCombo_;
    juce::Label          groupingLabel_;
    juce::ComboBox       groupingCombo_;
    juce::TextEditor     customGroupingEditor_;
    juce::TextButton     applyGroupingBtn_ {"Apply"};

    // Loop
    juce::TextButton     loop1Btn_     {"Loop 1"};
    juce::TextButton     loop2Btn_     {"Loop 2"};
    juce::TextButton     loopOffBtn_   {"Loop Off"};

    // Grid nudge
    juce::TextButton     gridLeftBtn_  {"..."};
    juce::TextButton     gridRightBtn_ {"..."};
    juce::Label          gridNudgeLabel_ {"", "Grid Nudge"};

    // Time display
    juce::Label          elapsedLabel_;
    juce::Label          remainingLabel_;

    // Waveform
    WaveformDisplay      waveform_;

    // Tempo ratio slider
    juce::Slider         tempoSlider_;
    juce::Label          tempoLabel_;

    djcore::TrackDataStore store_;

    // FileChooser must stay alive until callback fires
    std::shared_ptr<juce::FileChooser> activeChooser_;

    class RefreshTimer : public juce::Timer {
    public:
        DeckComponent* owner;
        void timerCallback() override { owner->updateUIFromDeck(); }
    } timer_;
};
