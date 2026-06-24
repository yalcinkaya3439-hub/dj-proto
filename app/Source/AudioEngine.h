#pragma once
#include <JuceHeader.h>
#include "djcore/WaveformAnalyzer.h"
#include "djcore/BPMAnalyzer.h"
#include "djcore/BeatGrid.h"
#include "djcore/LoopEngine.h"
#include "djcore/TrackData.h"
#include <atomic>
#include <functional>
#include <memory>
#include <string>

// ---- PlaybackState ---------------------------------------------------------
// All fields accessed from audio thread must be atomic or lock-free.
struct PlaybackState {
    std::atomic<double>  playheadSample{0.0};   // current sample position
    std::atomic<double>  tempoRatio{1.0};        // pitch-preserving speed multiplier
    std::atomic<bool>    playing{false};
    std::atomic<bool>    looping{false};
    std::atomic<double>  loopStart{0.0};
    std::atomic<double>  loopEnd{0.0};
    std::atomic<bool>    cueArmed{false};        // next press jumps to cue
    std::atomic<double>  cuePoint{0.0};
};

// ---- AudioDeck -------------------------------------------------------------
// One deck: owns its decoded audio buffer, waveform peaks, beatgrid, and
// the JUCE audio source that writes samples to the output.
class AudioDeck : private juce::AudioSource {
public:
    AudioDeck();
    ~AudioDeck() override;

    // Load a file (runs BPM analysis in a background thread)
    // onLoaded is called on the message thread when analysis is complete.
    void loadFile(const juce::File& file,
                  djcore::TrackDataStore& store,
                  std::function<void()> onLoaded);

    // Unload current file
    void unload();

    bool isLoaded() const  { return loaded_; }
    bool isPlaying() const { return state_.playing.load(); }

    // Transport
    void play();
    void pause();
    void stop();           // pause + return to cue
    void seekToSample(double sample);
    void setCuePoint(double sample);   // set cue
    void jumpToCue();
    void toggleLoop(int measures);
    void disableLoop();

    // Tempo
    void setTempoRatio(double ratio);  // 1.0 = original speed

    // BPM / grid mutators
    void setBPM(double bpm);
    void setTimeSignature(const djcore::TimeSignature& ts);
    void setBPMHalf();
    void setBPMDouble();
    void adjustBPM(double deltaBPM);
    void rerunBPMAnalysis();
    void shiftGrid(double deltaSamples);

    // Getters
    double           currentSamplePosition()   const;
    double           bpm()                     const;
    double           totalSamples()            const;
    int              sampleRate()              const;
    const djcore::WaveformPeaks& waveformPeaks() const { return peaks_; }
    const djcore::BeatGrid&      beatGrid()      const { return grid_; }
    const djcore::TrackAnalysis& trackAnalysis() const { return analysis_; }
    const PlaybackState&         playbackState() const { return state_; }

    // JUCE audio source — called by the device manager
    juce::AudioSource* audioSource() { return this; }

private:
    // juce::AudioSource interface
    void prepareToPlay(int samplesPerBlockExpected, double newSampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override;

    void doAnalysis(djcore::TrackDataStore& store, std::function<void()> onLoaded);

    // Audio data
    juce::AudioBuffer<float>   buffer_;
    bool                       loaded_        = false;
    int                        fileSampleRate_ = 44100;
    int                        outputSampleRate_ = 44100;

    // State
    PlaybackState              state_;

    // Analysis results
    djcore::WaveformPeaks      peaks_;
    djcore::BeatGrid           grid_;
    djcore::TrackAnalysis      analysis_;

    juce::CriticalSection      lock_;
    std::unique_ptr<juce::Thread> analysisThread_;
};

// ---- AudioEngine -----------------------------------------------------------
// Owns both decks and the JUCE device manager.
class AudioEngine : public juce::AudioIODeviceCallback {
public:
    AudioEngine();
    ~AudioEngine() override;

    void initialise();
    void shutdown();

    AudioDeck& deckA() { return *deckA_; }
    AudioDeck& deckB() { return *deckB_; }

    // Sync
    void applyTempoSync(AudioDeck& master, AudioDeck& follower);
    void applyBeatSync (AudioDeck& master, AudioDeck& follower);
    void applyBarSync  (AudioDeck& master, AudioDeck& follower);

    juce::AudioDeviceManager& deviceManager() { return deviceManager_; }

private:
    // juce::AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext(
        const float* const* inputChannelData, int numInputChannels,
        float* const* outputChannelData,      int numOutputChannels,
        int numSamples,
        const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    juce::AudioDeviceManager deviceManager_;
    juce::AudioMixer         mixer_;

    std::unique_ptr<AudioDeck> deckA_;
    std::unique_ptr<AudioDeck> deckB_;
};
