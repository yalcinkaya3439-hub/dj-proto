#include "AudioEngine.h"
#include <JuceHeader.h>
#include <cmath>

// ============================================================================
// AudioDeck
// ============================================================================

AudioDeck::AudioDeck() = default;
AudioDeck::~AudioDeck() {
    if (analysisThread_) analysisThread_->stopThread(5000);
}

void AudioDeck::loadFile(const juce::File& file,
                         djcore::TrackDataStore& store,
                         std::function<void()> onLoaded)
{
    if (analysisThread_) analysisThread_->stopThread(3000);

    state_.playing.store(false);
    state_.playheadSample.store(0.0);
    state_.looping.store(false);
    loaded_ = false;

    // Decode audio on a background thread
    struct AnalysisJob : public juce::Thread {
        AudioDeck* deck;
        juce::File file;
        djcore::TrackDataStore* store;
        std::function<void()> onLoaded;

        AnalysisJob(AudioDeck* d, juce::File f,
                    djcore::TrackDataStore* s, std::function<void()> cb)
            : Thread("AudioDeck_Analysis"), deck(d), file(std::move(f)),
              store(s), onLoaded(std::move(cb)) {}

        void run() override {
            deck->doAnalysis(*store, onLoaded);
        }
    };

    analysisThread_ = std::make_unique<AnalysisJob>(this, file, &store, onLoaded);
    analysisThread_->startThread();
}

void AudioDeck::doAnalysis(djcore::TrackDataStore& store,
                            std::function<void()> onLoaded)
{
    // Check data store first
    auto cached = store.load(analysis_.filePath);
    bool needAnalysis = !cached.has_value();

    // Decode audio file
    juce::AudioFormatManager fmtMgr;
    fmtMgr.registerBasicFormats();

    auto reader = std::unique_ptr<juce::AudioFormatReader>(
        fmtMgr.createReaderFor(juce::File(analysis_.filePath)));
    if (!reader) return;

    fileSampleRate_ = static_cast<int>(reader->sampleRate);
    int numChannels = static_cast<int>(reader->numChannels);
    auto numSamples = static_cast<juce::int64>(reader->lengthInSamples);

    {
        juce::ScopedLock sl(lock_);
        buffer_.setSize(numChannels, static_cast<int>(numSamples), false, true, false);
        reader->read(&buffer_, 0, static_cast<int>(numSamples), 0, true, true);
    }

    // Convert to interleaved float for core library
    std::vector<float> interleaved(numChannels * static_cast<size_t>(numSamples));
    for (juce::int64 s = 0; s < numSamples; ++s)
        for (int c = 0; c < numChannels; ++c)
            interleaved[static_cast<size_t>(s * numChannels + c)] =
                buffer_.getSample(c, static_cast<int>(s));

    // Compute waveform peaks
    peaks_ = djcore::WaveformAnalyzer::computePeaks(
        interleaved, numChannels, fileSampleRate_, 256);

    if (needAnalysis) {
        // BPM analysis
        auto bpmResult = djcore::BPMAnalyzer::analyze(
            interleaved, numChannels, fileSampleRate_);

        analysis_.detectedBPM    = bpmResult.bpm;
        analysis_.correctedBPM   = 0.0;
        analysis_.confidence     = bpmResult.confidence;
        analysis_.confidenceScore = bpmResult.confidenceScore;
        analysis_.firstBeatSample = bpmResult.firstBeatSample;
        analysis_.sampleRate     = fileSampleRate_;
        analysis_.totalSamples   = numSamples;
        analysis_.channels       = numChannels;
        analysis_.timeSig        = djcore::TimeSignature::make44();

        analysis_.fileHash = djcore::TrackDataStore::computeFileHash(analysis_.filePath);
        analysis_.modTime  = djcore::TrackDataStore::currentTimestamp();
        store.save(analysis_);
    } else {
        analysis_ = *cached;
    }

    // Build beat grid
    double bpm = analysis_.effectiveBPM();
    grid_ = djcore::BeatGrid(bpm, analysis_.firstBeatSample,
                             analysis_.timeSig, fileSampleRate_);
    grid_.recompute(numSamples);

    loaded_ = true;

    juce::MessageManager::callAsync([onLoaded] { onLoaded(); });
}

void AudioDeck::unload() {
    if (analysisThread_) analysisThread_->stopThread(3000);
    juce::ScopedLock sl(lock_);
    state_.playing.store(false);
    loaded_ = false;
    buffer_.setSize(0, 0);
}

void AudioDeck::play()  { if (loaded_) state_.playing.store(true); }
void AudioDeck::pause() { state_.playing.store(false); }
void AudioDeck::stop()  { state_.playing.store(false); jumpToCue(); }

void AudioDeck::seekToSample(double sample) {
    double clamped = std::max(0.0, std::min(sample,
        static_cast<double>(buffer_.getNumSamples() - 1)));
    state_.playheadSample.store(clamped);
}

void AudioDeck::setCuePoint(double sample) { state_.cuePoint.store(sample); }
void AudioDeck::jumpToCue() { seekToSample(state_.cuePoint.load()); }

void AudioDeck::toggleLoop(int measures) {
    if (state_.looping.load()) {
        disableLoop();
        return;
    }
    double pos = state_.playheadSample.load();
    auto region = djcore::LoopEngine::calculateMeasureLoop(
        pos, measures, grid_,
        djcore::QuantizeMode::MeasureStart,
        djcore::QuantizeSnap::Next);
    if (region.active) {
        state_.loopStart.store(region.startSample);
        state_.loopEnd.store(region.endSample);
        state_.looping.store(true);
    }
}

void AudioDeck::disableLoop() { state_.looping.store(false); }

void AudioDeck::setTempoRatio(double ratio) {
    state_.tempoRatio.store(std::max(0.25, std::min(4.0, ratio)));
}

void AudioDeck::setBPM(double bpm) {
    analysis_.correctedBPM = bpm;
    grid_.setBPM(bpm);
    grid_.recompute(buffer_.getNumSamples());
}

void AudioDeck::setTimeSignature(const djcore::TimeSignature& ts) {
    analysis_.timeSig = ts;
    grid_.setTimeSig(ts);
    grid_.recompute(buffer_.getNumSamples());
}

void AudioDeck::setBPMHalf()   { setBPM(analysis_.effectiveBPM() / 2.0); }
void AudioDeck::setBPMDouble() { setBPM(analysis_.effectiveBPM() * 2.0); }
void AudioDeck::adjustBPM(double delta) { setBPM(analysis_.effectiveBPM() + delta); }

void AudioDeck::rerunBPMAnalysis() {
    // Trigger a new analysis without clearing loaded audio
    // Simplified: just reset correctedBPM so next load re-analyses
    analysis_.correctedBPM = 0.0;
}

void AudioDeck::shiftGrid(double deltaSamples) {
    grid_.shiftGrid(deltaSamples);
    analysis_.firstBeatSample += deltaSamples;
}

double AudioDeck::currentSamplePosition() const { return state_.playheadSample.load(); }
double AudioDeck::bpm()                   const { return analysis_.effectiveBPM(); }
double AudioDeck::totalSamples()          const { return static_cast<double>(buffer_.getNumSamples()); }
int    AudioDeck::sampleRate()            const { return outputSampleRate_; }

// ---- JUCE AudioSource callbacks --------------------------------------------
void AudioDeck::prepareToPlay(int /*samplesPerBlockExpected*/, double newSampleRate) {
    outputSampleRate_ = static_cast<int>(newSampleRate);
}

void AudioDeck::releaseResources() {}

void AudioDeck::getNextAudioBlock(const juce::AudioSourceChannelInfo& info) {
    info.clearActiveBufferRegion();
    if (!loaded_ || !state_.playing.load()) return;

    juce::ScopedLock sl(lock_);

    int numChannels = buffer_.getNumChannels();
    int totalBufSamples = buffer_.getNumSamples();
    double pos = state_.playheadSample.load();
    double ratio = state_.tempoRatio.load();

    // Simple non-interpolated playback with tempo ratio
    // For a production implementation, use a resampling algorithm (e.g. JUCE ResamplingAudioSource)
    for (int i = 0; i < info.numSamples; ++i) {
        int srcSample = static_cast<int>(pos);

        if (state_.looping.load()) {
            double loopEnd = state_.loopEnd.load();
            if (pos >= loopEnd) {
                pos = state_.loopStart.load();
                srcSample = static_cast<int>(pos);
            }
        }

        if (srcSample >= totalBufSamples) {
            state_.playing.store(false);
            break;
        }

        for (int c = 0; c < info.buffer->getNumChannels(); ++c) {
            int srcCh = c % numChannels;
            float sample = buffer_.getSample(srcCh, srcSample);
            info.buffer->addSample(c, info.startSample + i, sample);
        }

        pos += ratio;
    }

    state_.playheadSample.store(pos);
}

// ============================================================================
// AudioEngine
// ============================================================================

AudioEngine::AudioEngine()
    : deckA_(std::make_unique<AudioDeck>()),
      deckB_(std::make_unique<AudioDeck>())
{}

AudioEngine::~AudioEngine() { shutdown(); }

void AudioEngine::initialise() {
    auto result = deviceManager_.initialiseWithDefaultDevices(0, 2);
    if (result.isNotEmpty()) {
        // Try fallback: software output
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        deviceManager_.getAudioDeviceSetup(setup);
        setup.outputDeviceName = "";
        deviceManager_.setAudioDeviceSetup(setup, true);
    }
    deviceManager_.addAudioCallback(this);
}

void AudioEngine::shutdown() {
    deviceManager_.removeAudioCallback(this);
    deckA_->unload();
    deckB_->unload();
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device) {
    double sr = device->getCurrentSampleRate();
    int block = device->getCurrentBufferSizeSamples();
    deckA_->prepareToPlay(block, sr);
    deckB_->prepareToPlay(block, sr);
}

void AudioEngine::audioDeviceStopped() {
    deckA_->releaseResources();
    deckB_->releaseResources();
}

void AudioEngine::audioDeviceIOCallbackWithContext(
    const float* const* /*inputChannelData*/, int /*numInputChannels*/,
    float* const* outputChannelData, int numOutputChannels,
    int numSamples,
    const juce::AudioIODeviceCallbackContext& /*context*/)
{
    juce::AudioBuffer<float> outBuf(outputChannelData, numOutputChannels, numSamples);
    outBuf.clear();

    // Mix both decks into the output
    juce::AudioBuffer<float> tmp(numOutputChannels, numSamples);

    auto fillDeck = [&](AudioDeck& deck) {
        tmp.clear();
        juce::AudioSourceChannelInfo info(&tmp, 0, numSamples);
        deck.audioSource()->getNextAudioBlock(info);
        for (int ch = 0; ch < numOutputChannels; ++ch)
            outBuf.addFrom(ch, 0, tmp, ch, 0, numSamples);
    };

    fillDeck(*deckA_);
    fillDeck(*deckB_);

    // Simple soft clip
    for (int ch = 0; ch < numOutputChannels; ++ch) {
        auto* data = outBuf.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] = std::tanh(data[i]);
    }
}

// ---- Sync helpers ----------------------------------------------------------
void AudioEngine::applyTempoSync(AudioDeck& master, AudioDeck& follower) {
    // Only adjusts playback speed — does NOT move playhead
    double masterBPM   = master.bpm();
    double followerBPM = follower.bpm();
    if (followerBPM <= 0.0 || masterBPM <= 0.0) return;
    double ratio = masterBPM / followerBPM;
    follower.setTempoRatio(ratio);
}

void AudioEngine::applyBeatSync(AudioDeck& master, AudioDeck& follower) {
    applyTempoSync(master, follower);
    // Phase alignment: find nearest sub-beat in follower that aligns with master
    const djcore::BeatGrid& mGrid = master.beatGrid();
    const djcore::BeatGrid& fGrid = follower.beatGrid();
    double mPos = master.currentSamplePosition();
    double fPos = follower.currentSamplePosition();

    // Find master's phase within a beat
    double mSbDur = mGrid.subBeatDurationSamples();
    if (mSbDur <= 0.0) return;
    double mPhase = std::fmod(mPos - mGrid.getFirstBeat(), mSbDur);
    if (mPhase < 0.0) mPhase += mSbDur;

    // Adjust follower playhead so its phase matches
    double fSbDur = fGrid.subBeatDurationSamples();
    if (fSbDur <= 0.0) return;
    double fPhaseOffset = std::fmod(fPos - fGrid.getFirstBeat(), fSbDur);
    if (fPhaseOffset < 0.0) fPhaseOffset += fSbDur;

    double correction = (mPhase - fPhaseOffset) * (fSbDur / mSbDur);
    follower.seekToSample(fPos + correction);
}

void AudioEngine::applyBarSync(AudioDeck& master, AudioDeck& follower) {
    // Bar sync only valid when time signatures match
    const djcore::TimeSignature& mTS = master.beatGrid().getTimeSig();
    const djcore::TimeSignature& fTS = follower.beatGrid().getTimeSig();
    if (mTS.numerator != fTS.numerator || mTS.denominator != fTS.denominator) {
        // Safety: fall back to tempo-only sync
        applyTempoSync(master, follower);
        return;
    }
    applyTempoSync(master, follower);

    // Align measure starts
    const djcore::BeatGrid& mGrid = master.beatGrid();
    const djcore::BeatGrid& fGrid = follower.beatGrid();
    double mPos = master.currentSamplePosition();
    double mMeasDur = mGrid.measureDurationSamples();
    double fMeasDur = fGrid.measureDurationSamples();
    if (mMeasDur <= 0.0 || fMeasDur <= 0.0) return;

    double mPhase = std::fmod(mPos - mGrid.getFirstBeat(), mMeasDur);
    if (mPhase < 0.0) mPhase += mMeasDur;

    double fPos = follower.currentSamplePosition();
    double fFirstBeat = fGrid.getFirstBeat();
    double fPhase = std::fmod(fPos - fFirstBeat, fMeasDur);
    if (fPhase < 0.0) fPhase += fMeasDur;

    // Move follower playhead so its measure phase matches master
    double correction = (mPhase * (fMeasDur / mMeasDur)) - fPhase;
    follower.seekToSample(fPos + correction);
}
