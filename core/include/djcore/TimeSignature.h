#pragma once
#include <vector>
#include <string>
#include <stdexcept>

namespace djcore {

struct TimeSignature {
    int numerator   = 4;  // e.g. 7
    int denominator = 4;  // e.g. 8
    std::vector<int> grouping;  // e.g. {2,2,3} for 7/8

    // Returns true if grouping is non-empty and sums to numerator
    bool isValid() const;

    // Parse "2+2+3" → {2,2,3}. Throws std::invalid_argument on bad input.
    static std::vector<int> parseGrouping(const std::string& str);

    // Format grouping as "2+2+3"
    std::string groupingString() const;

    // Duration of one sub-beat in seconds given BPM.
    // Sub-beat = denominator note (e.g. eighth-note for /8, quarter-note for /4).
    // BPM counts denominator notes per minute.
    double subBeatDurationSeconds(double bpm) const;

    // Duration of one full measure in seconds
    double measureDurationSeconds(double bpm) const;

    // Duration in samples
    double subBeatDurationSamples(double bpm, int sampleRate) const;
    double measureDurationSamples(double bpm, int sampleRate) const;

    // ---- Preset factories ----
    static TimeSignature make24();
    static TimeSignature make34();
    static TimeSignature make44();
    static TimeSignature make58();
    static TimeSignature make68();
    static TimeSignature make78(std::vector<int> grouping = {2, 2, 3});
    static TimeSignature make88();
    static TimeSignature make98(std::vector<int> grouping = {2, 2, 2, 3});
    static TimeSignature make108();
    static TimeSignature make128();

    // Validate that a grouping vector is legal for this signature
    bool isGroupingValid(const std::vector<int>& g) const;
};

} // namespace djcore
