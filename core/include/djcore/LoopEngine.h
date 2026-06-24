#pragma once
#include "BeatGrid.h"

namespace djcore {

enum class QuantizeMode {
    Off,
    SubBeat,
    GroupStart,
    MeasureStart,
    PhraseStart
};

enum class QuantizeSnap {
    Nearest,  // snap to nearest grid point (before or after)
    Next      // snap to next grid point strictly after current position
};

struct LoopRegion {
    double startSample = 0.0;
    double endSample   = 0.0;  // exclusive — loop jumps back here
    bool   active      = false;
};

class LoopEngine {
public:
    // Quantize a sample position to a grid point.
    // Mode=Off returns pos unchanged.
    static double quantize(
        double pos,
        const BeatGrid& grid,
        QuantizeMode mode,
        QuantizeSnap snap = QuantizeSnap::Nearest);

    // Calculate a loop of `measures` full measures starting at or near pos.
    // The start is first quantized to a MeasureStart (or nearest if Off).
    static LoopRegion calculateMeasureLoop(
        double pos,
        int measures,
        const BeatGrid& grid,
        QuantizeMode startMode = QuantizeMode::MeasureStart,
        QuantizeSnap snap      = QuantizeSnap::Next);

    // Calculate a loop of exactly N sub-beats (useful for 1-beat, group-length loops)
    static LoopRegion calculateSubBeatLoop(
        double pos,
        int subBeats,
        const BeatGrid& grid,
        QuantizeMode startMode = QuantizeMode::SubBeat,
        QuantizeSnap snap      = QuantizeSnap::Next);
};

} // namespace djcore
