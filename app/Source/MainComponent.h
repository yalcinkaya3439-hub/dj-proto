#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"
#include "DeckComponent.h"

class MainComponent : public juce::Component {
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    AudioEngine engine_;

    DeckComponent deckA_;
    DeckComponent deckB_;

    // Sync controls
    juce::Label         syncLabel_;
    juce::TextButton    tempoSyncBtn_ {"Tempo Sync"};
    juce::TextButton    beatSyncBtn_  {"Beat Sync"};
    juce::TextButton    barSyncBtn_   {"Bar Sync"};
    juce::ToggleButton  masterAToggle_{"A Master"};
    juce::ToggleButton  masterBToggle_{"B Master"};

    bool masterIsA_ = true;

    void applySyncAction(juce::TextButton& btn);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
