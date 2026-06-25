#include "AudioEngine.h"
#include <JuceHeader.h>
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// AudioDeck
// ============================================================================

AudioDeck::AudioDeck() = default;

AudioDeck::~AudioDeck() {
    if (analysisThread_) analysisThread_->stopThread(5000);
}

void AudioDeck::loadFile(const juce::File& file,
                         djcore::TrackDataStore& store,
                         std::function<void(LoadResult)> onLoaded)
{
    if (analysisThread_) analysisThread_->stopThread(3000);

    state_.playing.store(false);
    state_.playheadSample.store(0.0);
    state_.looping.store(false);
    loaded_ = false;

    // --- KEY FIX: store the file path BEFORE the background thread starts ---
    analysis_.filePath = file.getFullPathName().toStdString();

    struct AnalysisJob : public juce::Thread {
        AudioDeck*                       deck;
        juce::File                       file;
        djcore::TrackDataStore*          store;
        std::function<void(LoadResult)>  onLoaded;

        AnalysisJob(AudioDeck* d, juce::File f,
                    djcore::TrackDataStore* s,
                    std::function<void(LoadResult)> cb)
            : Thread("AudioDeck_Analysis"),
              deck(d), file(std::move(f)),
              store(s), onLoaded(std::move(cb)) {}

        void run() override {
            deck->doAnalysis(file, *store, onLoaded);
        }
    };

    analysisThread_ = std::make_unique<AnalysisJob>(this, file, &store, onLoaded);
    analysisThread_->startThread();
}

void AudioDeck::doAnalysis(const juce::File& file,
                            djcore::TrackDataStore& store,
                            std::function<void(LoadResult)> onLoaded)
{
    // Helper to fire callback on the message thread
    auto fireCallback = [&](LoadResult res) {
        juce::MessageManager::callAsync([onLoaded, res] { onLoaded(res); });
    };

    // --- 1. Validate file exists and is readable ---
    if (!file.existsAsFile()) {
        LoadResult r;
        r.ok    = false;
        r.error = "File not found: " + file.getFullPathName();
        fireCallback(r);
        return;
    }

    // --- 2. Build format manager (WAV first, then all basics) ---
    juce::AudioFormatManager fmtMgr;
    fmtMgr.registerBasicFormats();   // WAV, AIFF, FLAC, MP3 (if available)

    auto reader = std::unique_ptr<juce::AudioFormatReader>(
        fmtMgr.createReaderFor(file));

    if (!reader) {
        LoadResult r;
        r.ok       = false;
        r.fileName = file.getFileName();
        r.error    = "Unsupported format or corrupt file: " + file.getFileName()
                   + "\nSupported: WAV, AIFF, FLAC";
        fireCallback(r);
        return;
    }

    // --- 3. Decode audio into buffer ---
    fileSampleRate_    = static_cast<int>(reader->sampleRate);
    int    numChannels = static_cast<int>(reader->numChannels);
    auto   numSamples  = static_cast<juce::int64>(reader->lengthInSamples);

    if (numSamples <= 0 || numChannels <= 0 || fileSampleRate_ <= 0) {
        LoadResult r;
        r.ok    = false;
        r.error = "Invalid audio data in file: " + file.getFileName();
        fireCallback(r);
        return;
    }

    {
        juce::ScopedLock sl(lock_);
        buffer_.setSize(numChannels, static_cast<int>(numSamples), false, true, false);
        reader->read(&buffer_, 0, static_cast<int>(numSamples), 0, true, true);
    }

    // --- 4. Build interleaved float vector for core library ---
    std::vector<float> interleaved(
        static_cast<size_t>(numChannels) * static_cast<size_t>(numSamples));
    for (juce::int64 s = 0; s < numSamples; ++s)
        for (int c = 0; c < numChannels; ++c)
            interleaved[static_cast<size_t>(s * numChannels + c)] =
                buffer_.getSample(c, static_cast<int>(s));

    // --- 5. Compute waveform peaks ---
    peaks_ = djcore::WaveformAnalyzer::computePeaks(
        interleaved, numChannels, fileSampleRate_, 256);

    // --- 6. BPM analysis (use cache if available) ---
    auto cached = store.load(analysis_.filePath);
    if (cached.has_value()) {
        analysis_ = *cached;
    } else {
        auto bpmResult = djcore::BPMAnalyzer::analyze(
            interleaved, numChannels, fileSampleRate_);

        analysis_.detectedBPM     = bpmResult.bpm;
        analysis_.correctedBPM    = 0.0;
        analysis_.confidence      = bpmResult.confidence;
        analysis_.confidenceScore = bpmResult.confidenceScore;
        analysis_.firstBeatSample = bpmResult.firstBeatSample;
        analysis_.sampleRate      = fileSampleRate_;
        analysis_.totalSamples    = numSamples;
        analysis_.channels        = numChannels;
        analysis_.timeSig         = djcore::TimeSignature::make44();
        analysis_.filePath        = file.getFullPathName().toStdString();
        analysis_.fileHash        = djcore::TrackDataStore::computeFileHash(analysis_.filePath);
        analysis_.modTime         = djcore::TrackDataStore::currentTimestamp();
        store.save(analysis_);
    }

    // --- 7. Build beat grid ---
    double bpm = analysis_.effectiveBPM();
    grid_ = djcore::BeatGrid(bpm, analysis_.firstBeatSample,
                             analysis_.timeSig, fileSampleRate_);
    grid_.recompute(numSamples);

    // --- 8. Mark loaded ---
    loaded_ = true;

    // --- 9. Fire success callback on message thread ---
    LoadResult res;
    res.ok          = true;
    res.fileName    = file.getFileName();
    res.bpm         = bpm;
    res.sampleRate  = fileSampleRate_;
    res.durationSec = static_cast<double>(numSamples) / fileSampleRate_;
    fireCallback(res);
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
    double clamped = std::max(0.0,
        std::min(sample, static_cast<double>(buffer_.getNumSamples() - 1)));
    state_.playheadSample.store(clamped);
}

void AudioDeck::setCuePoint(double sample) { state_.cuePoint.store(sample); }
void AudioDeck::jumpToCue() { seekToSample(state_.cuePoint.load()); }

void AudioDeck::toggleLoop(int measures) {
    if (state_.looping.load()) { disableLoop(); return; }
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

void AudioDeck::setBPMHalf()              { setBPM(analysis_.effectiveBPM() / 2.0); }
void AudioDeck::setBPMDouble()            { setBPM(analysis_.effectiveBPM() * 2.0); }
void AudioDeck::adjustBPM(double delta)   { setBPM(analysis_.effectiveBPM() + delta); }

void AudioDeck::rerunBPMAnalysis() {
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

// ---- juce::AudioSource callbacks -------------------------------------------

void AudioDeck::prepareToPlay(int /*samplesPerBlockExpected*/, double newSampleRate) {
    outputSampleRate_ = static_cast<int>(newSampleRate);
}

void AudioDeck::releaseResources() {}

void AudioDeck::getNextAudioBlock(const juce::AudioSourceChannelInfo& info) {
    info.clearActiveBufferRegion();
    if (!loaded_ || !state_.playing.load()) return;

    juce::ScopedLock sl(lock_);

    int    numChannels      = buffer_.getNumChannels();
    int    totalBufSamples  = buffer_.getNumSamples();
    double pos              = state_.playheadSample.load();
    double ratio            = state_.tempoRatio.load();

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
            int   srcCh  = c % numChannels;
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
    double sr    = device->getCurrentSampleRate();
    int    block = device->getCurrentBufferSizeSamples();
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

    juce::AudioBuffer<float> tmp(numOutputChannels, numSamples);

    auto fillDeck = [&](AudioDeck& deck) {
        tmp.clear();
        juce::AudioSourceChannelInfo info(&tmp, 0, numSamples);
        deck.getNextAudioBlock(info);
        for (int ch = 0; ch < numOutputChannels; ++ch)
            outBuf.addFrom(ch, 0, tmp, ch, 0, numSamples);
    };

    fillDeck(*deckA_);
    fillDeck(*deckB_);

    // Soft clip
    for (int ch = 0; ch < numOutputChannels; ++ch) {
        auto* data = outBuf.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] = std::tanh(data[i]);
    }
}

// ---- Sync helpers ----------------------------------------------------------

void AudioEngine::applyTempoSync(AudioDeck& master, AudioDeck& follower) {
    double masterBPM   = master.bpm();
    double followerBPM = follower.bpm();
    if (followerBPM <= 0.0 || masterBPM <= 0.0) return;
    follower.setTempoRatio(masterBPM / followerBPM);
}

void AudioEngine::applyBeatSync(AudioDeck& master, AudioDeck& follower) {
    applyTempoSync(master, follower);

    const djcore::BeatGrid& mGrid = master.beatGrid();
    const djcore::BeatGrid& fGrid = follower.beatGrid();
    double mPos   = master.currentSamplePosition();
    double fPos   = follower.currentSamplePosition();
    double mSbDur = mGrid.subBeatDurationSamples();
    if (mSbDur <= 0.0) return;

    double mPhase = std::fmod(mPos - mGrid.getFirstBeat(), mSbDur);
    if (mPhase < 0.0) mPhase += mSbDur;

    double fSbDur = fGrid.subBeatDurationSamples();
    if (fSbDur <= 0.0) return;

    double fPhaseOffset = std::fmod(fPos - fGrid.getFirstBeat(), fSbDur);
    if (fPhaseOffset < 0.0) fPhaseOffset += fSbDur;

    double correction = (mPhase - fPhaseOffset) * (fSbDur / mSbDur);
    follower.seekToSample(fPos + correction);
}

void AudioEngine::applyBarSync(AudioDeck& master, AudioDeck& follower) {
    const djcore::TimeSignature& mTS = master.beatGrid().getTimeSig();
    const djcore::TimeSignature& fTS = follower.beatGrid().getTimeSig();
    if (mTS.numerator != fTS.numerator || mTS.denominator != fTS.denominator) {
        applyTempoSync(master, follower);
        return;
    }
    applyTempoSync(master, follower);

    const djcore::BeatGrid& mGrid  = master.beatGrid();
    const djcore::BeatGrid& fGrid  = follower.beatGrid();
    double mPos     = master.currentSamplePosition();
    double mMeasDur = mGrid.measureDurationSamples();
    double fMeasDur = fGrid.measureDurationSamples();
    if (mMeasDur <= 0.0 || fMeasDur <= 0.0) return;

    double mPhase = std::fmod(mPos - mGrid.getFirstBeat(), mMeasDur);
    if (mPhase < 0.0) mPhase += mMeasDur;

    double fPos      = follower.currentSamplePosition();
    double fPhase    = std::fmod(fPos - fGrid.getFirstBeat(), fMeasDur);
    if (fPhase < 0.0) fPhase += fMeasDur;

    double correction = (mPhase * (fMeasDur / mMeasDur)) - fPhase;
    follower.seekToSample(fPos + correction);
}
