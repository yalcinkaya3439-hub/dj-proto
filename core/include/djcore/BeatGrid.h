#pragma once
#include "TimeSignature.h"
#include <vector>
#include <cstdint>

namespace djcore {

enum class BeatLevel {
    SubBeat,      // every denominator-note (e.g. every eighth note in 7/8)
    GroupStart,   // first sub-beat of each grouping group (e.g. beat 1 of 2+2+3)
    MeasureStart, // first sub-beat of each measure
    PhraseStart   // user-defined phrase boundaries
};

struct BeatMark {
    double   samplePosition = 0.0;
    BeatLevel level          = BeatLevel::SubBeat;
    int      subBeatIndex   = 0;  // 0-based index within the current measure
    int      groupIndex     = 0;  // 0-based group index within the measure
    int      measureIndex   = 0;  // 0-based absolute measure number
};

struct GridAnchor {
    double samplePosition = 0.0;
    double bpm            = 120.0;  // BPM starting at this sample (tempo change support)
};

class BeatGrid {
public:
    BeatGrid() = default;
    BeatGrid(double bpm,
             double firstBeatSample,
             const TimeSignature& timeSig,
             int sampleRate);

    // Re-generate all BeatMarks up to totalSamples.
    // Call this after any parameter change.
    void recompute(int64_t totalSamples);

    // ---- Queries ----
    // All marks within [startSample, endSample)
    std::vector<BeatMark> getMarksInRange(double startSample, double endSample) const;

    // Nearest mark at or before pos with at least minLevel importance
    BeatMark nearestAtOrBefore(double pos, BeatLevel minLevel) const;

    // Next mark strictly after pos with at least minLevel importance
    BeatMark nextAfter(double pos, BeatLevel minLevel) const;

    // Durations in samples
    double subBeatDurationSamples() const;
    double measureDurationSamples() const;
    double loopLengthSamples(int measures) const;

    // ---- Mutators ----
    void setBPM(double bpm);
    void setFirstBeatSample(double s);
    void setTimeSig(const TimeSignature& ts);
    void shiftGrid(double deltaSamples);          // nudge all marks
    void addAnchor(const GridAnchor& anchor);     // for variable-tempo sections
    void clearAnchors();

    // ---- Phrase marks (user-defined) ----
    void setPhraseStarts(const std::vector<double>& samples);
    const std::vector<double>& phraseStarts() const { return phraseStarts_; }

    // ---- Getters ----
    double              getBPM()            const { return bpm_; }
    double              getFirstBeat()      const { return firstBeatSample_; }
    const TimeSignature& getTimeSig()       const { return timeSig_; }
    int                 getSampleRate()     const { return sampleRate_; }
    const std::vector<BeatMark>& getMarks() const { return marks_; }

private:
    double         bpm_             = 120.0;
    double         firstBeatSample_ = 0.0;
    TimeSignature  timeSig_;
    int            sampleRate_      = 44100;
    std::vector<GridAnchor> anchors_;
    std::vector<double>     phraseStarts_;
    std::vector<BeatMark>   marks_;
};

} // namespace djcore
