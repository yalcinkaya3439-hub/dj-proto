#include "djcore/LoopEngine.h"
#include <cmath>
#include <limits>

namespace djcore {

static BeatLevel quantizeLevelFromMode(QuantizeMode mode) {
    switch (mode) {
        case QuantizeMode::SubBeat:      return BeatLevel::SubBeat;
        case QuantizeMode::GroupStart:   return BeatLevel::GroupStart;
        case QuantizeMode::MeasureStart: return BeatLevel::MeasureStart;
        case QuantizeMode::PhraseStart:  return BeatLevel::PhraseStart;
        default:                         return BeatLevel::SubBeat;
    }
}

double LoopEngine::quantize(
    double pos,
    const BeatGrid& grid,
    QuantizeMode mode,
    QuantizeSnap snap)
{
    if (mode == QuantizeMode::Off) return pos;

    BeatLevel lvl = quantizeLevelFromMode(mode);

    if (snap == QuantizeSnap::Next) {
        BeatMark next = grid.nextAfter(pos, lvl);
        if (next.samplePosition == std::numeric_limits<double>::max())
            return pos;  // No next mark found
        return next.samplePosition;
    }

    // Nearest: pick closest of before/after
    BeatMark before = grid.nearestAtOrBefore(pos, lvl);
    BeatMark after  = grid.nextAfter(pos, lvl);

    double distBefore = pos - before.samplePosition;
    double distAfter  = (after.samplePosition == std::numeric_limits<double>::max())
                        ? std::numeric_limits<double>::max()
                        : after.samplePosition - pos;

    return (distBefore <= distAfter) ? before.samplePosition : after.samplePosition;
}

LoopRegion LoopEngine::calculateMeasureLoop(
    double pos,
    int measures,
    const BeatGrid& grid,
    QuantizeMode startMode,
    QuantizeSnap snap)
{
    LoopRegion loop;
    loop.active = false;

    double loopLen = grid.loopLengthSamples(measures);
    if (loopLen <= 0.0) return loop;

    double start;
    if (startMode == QuantizeMode::Off) {
        start = pos;
    } else {
        start = quantize(pos, grid, startMode, snap);
    }

    loop.startSample = start;
    loop.endSample   = start + loopLen;
    loop.active      = true;
    return loop;
}

LoopRegion LoopEngine::calculateSubBeatLoop(
    double pos,
    int subBeats,
    const BeatGrid& grid,
    QuantizeMode startMode,
    QuantizeSnap snap)
{
    LoopRegion loop;
    loop.active = false;

    double sbDur = grid.subBeatDurationSamples();
    if (sbDur <= 0.0 || subBeats <= 0) return loop;

    double start;
    if (startMode == QuantizeMode::Off) {
        start = pos;
    } else {
        start = quantize(pos, grid, startMode, snap);
    }

    loop.startSample = start;
    loop.endSample   = start + subBeats * sbDur;
    loop.active      = true;
    return loop;
}

} // namespace djcore
