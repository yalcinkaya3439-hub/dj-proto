#include "MainComponent.h"

MainComponent::MainComponent()
    : deckA_(engine_.deckA(), "Deck A"),
      deckB_(engine_.deckB(), "Deck B")
{
    engine_.initialise();

    syncLabel_.setText("Sync", juce::dontSendNotification);
    syncLabel_.setFont(juce::Font(16.0f, juce::Font::bold));
    syncLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(syncLabel_);

    for (auto* btn : {&tempoSyncBtn_, &beatSyncBtn_, &barSyncBtn_}) {
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1565c0));
        btn->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        btn->onClick = [this, btn] { applySyncAction(*btn); };
        addAndMakeVisible(btn);
    }

    masterAToggle_.setToggleState(true, juce::dontSendNotification);
    masterBToggle_.setToggleState(false, juce::dontSendNotification);
    masterAToggle_.setColour(juce::ToggleButton::textColourId, juce::Colours::lightgrey);
    masterBToggle_.setColour(juce::ToggleButton::textColourId, juce::Colours::lightgrey);

    masterAToggle_.onClick = [this] {
        masterIsA_ = true;
        masterBToggle_.setToggleState(false, juce::dontSendNotification);
        masterAToggle_.setToggleState(true, juce::dontSendNotification);
    };
    masterBToggle_.onClick = [this] {
        masterIsA_ = false;
        masterAToggle_.setToggleState(false, juce::dontSendNotification);
        masterBToggle_.setToggleState(true, juce::dontSendNotification);
    };

    addAndMakeVisible(masterAToggle_);
    addAndMakeVisible(masterBToggle_);
    addAndMakeVisible(deckA_);
    addAndMakeVisible(deckB_);

    setSize(1280, 780);
}

MainComponent::~MainComponent() {
    engine_.shutdown();
}

void MainComponent::applySyncAction(juce::TextButton& btn) {
    AudioDeck& master   = masterIsA_ ? engine_.deckA() : engine_.deckB();
    AudioDeck& follower = masterIsA_ ? engine_.deckB() : engine_.deckA();

    if (&btn == &tempoSyncBtn_) {
        engine_.applyTempoSync(master, follower);
    } else if (&btn == &beatSyncBtn_) {
        engine_.applyBeatSync(master, follower);
    } else if (&btn == &barSyncBtn_) {
        // Safety check: warn if time signatures differ
        auto& mTS = master.beatGrid().getTimeSig();
        auto& fTS = follower.beatGrid().getTimeSig();
        if (mTS.numerator != fTS.numerator || mTS.denominator != fTS.denominator) {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Bar Sync Warning",
                juce::String("Decks have different time signatures (") +
                juce::String(mTS.numerator) + "/" + juce::String(mTS.denominator) + " vs " +
                juce::String(fTS.numerator) + "/" + juce::String(fTS.denominator) +
                ").\n\nBar Sync is disabled. Use Tempo Sync or Beat Sync instead.");
            return;
        }
        engine_.applyBarSync(master, follower);
    }
}

void MainComponent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff0d1117));
}

void MainComponent::resized() {
    auto area = getLocalBounds();

    // Sync strip at bottom
    auto syncStrip = area.removeFromBottom(44);
    syncStrip = syncStrip.reduced(8, 4);
    syncLabel_.setBounds(syncStrip.removeFromLeft(50));
    masterAToggle_.setBounds(syncStrip.removeFromLeft(80));
    masterBToggle_.setBounds(syncStrip.removeFromLeft(80));
    syncStrip.removeFromLeft(12);
    tempoSyncBtn_.setBounds(syncStrip.removeFromLeft(100));
    syncStrip.removeFromLeft(4);
    beatSyncBtn_.setBounds(syncStrip.removeFromLeft(100));
    syncStrip.removeFromLeft(4);
    barSyncBtn_.setBounds(syncStrip.removeFromLeft(100));

    // Two decks side-by-side
    int half = area.getWidth() / 2;
    deckA_.setBounds(area.removeFromLeft(half));
    deckB_.setBounds(area);
}
