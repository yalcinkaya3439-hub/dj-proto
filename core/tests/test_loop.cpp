#include <catch2/catch.hpp>
#include "djcore/LoopEngine.h"
#include "djcore/BeatGrid.h"
#include "djcore/TimeSignature.h"
#include <cmath>

using namespace djcore;
static constexpr int SR = 44100;

// ---- Helpers ---------------------------------------------------------------
static BeatGrid makeGrid(const TimeSignature& ts, double bpm, double firstBeat = 0.0) {
    BeatGrid g(bpm, firstBeat, ts, SR);
    g.recompute(SR * 60);
    return g;
}

// ---- 4/4 basic loop --------------------------------------------------------
TEST_CASE("4/4 at 120 BPM: 1-measure loop length = 4 quarter-notes", "[loop][4/4]") {
    auto ts = TimeSignature::make44();
    auto g  = makeGrid(ts, 120.0);
    // quarter-note = 22050 samples, measure = 88200 samples
    double expected = 4.0 * (60.0 / 120.0) * SR;
    REQUIRE(g.loopLengthSamples(1) == Approx(expected).margin(0.5));
}

TEST_CASE("4/4 at 120 BPM: 2-measure loop = 2 x 1-measure loop", "[loop][4/4]") {
    auto ts = TimeSignature::make44();
    auto g  = makeGrid(ts, 120.0);
    REQUIRE(g.loopLengthSamples(2) == Approx(2.0 * g.loopLengthSamples(1)).margin(0.5));
}

// ---- 7/8 loop correctness --------------------------------------------------
TEST_CASE("7/8 at 140 BPM: 1-measure loop = exactly 7 eighth-notes", "[loop][7/8]") {
    auto ts = TimeSignature::make78({2,2,3});
    auto g  = makeGrid(ts, 140.0);
    double eigthNoteSamples = 60.0 / 140.0 * SR;
    double expected         = 7.0 * eigthNoteSamples;
    REQUIRE(g.loopLengthSamples(1) == Approx(expected).margin(0.5));
}

TEST_CASE("7/8 NOT 8-note measure: 7/8 measure has 7 sub-beats, not 8", "[loop][7/8]") {
    // Both grids use eighth-note BPM.
    // 7/8 = 7 eighth-notes per measure; 8/8 = 8 eighth-notes per measure.
    // At the same BPM (same eighth-note speed), 7/8 measure is SHORTER than 8/8.
    auto ts78 = TimeSignature::make78({2,2,3});
    auto ts88 = TimeSignature::make88();
    auto g78  = makeGrid(ts78, 120.0);
    auto g88  = makeGrid(ts88, 120.0);
    // 7/8 measure = 7 × (60/120) × 44100 = 154350 samples
    // 8/8 measure = 8 × (60/120) × 44100 = 176400 samples
    REQUIRE(g78.loopLengthSamples(1) < g88.loopLengthSamples(1));
    // Verify the difference is exactly 1 eighth-note (one sub-beat)
    double sbDur = g78.subBeatDurationSamples();
    REQUIRE(g88.loopLengthSamples(1) - g78.loopLengthSamples(1) == Approx(sbDur).margin(0.5));
}

// ---- 9/8 loop correctness --------------------------------------------------
TEST_CASE("9/8 at 120 BPM: 1-measure loop = exactly 9 eighth-notes", "[loop][9/8]") {
    auto ts = TimeSignature::make98({2,2,2,3});
    auto g  = makeGrid(ts, 120.0);
    double eigthNoteSamples = 60.0 / 120.0 * SR;
    double expected         = 9.0 * eigthNoteSamples;
    REQUIRE(g.loopLengthSamples(1) == Approx(expected).margin(0.5));
}

TEST_CASE("9/8 at 120 BPM: 2-measure loop = 18 eighth-notes", "[loop][9/8]") {
    auto ts = TimeSignature::make98({2,2,2,3});
    auto g  = makeGrid(ts, 120.0);
    double eigthNoteSamples = 60.0 / 120.0 * SR;
    double expected         = 18.0 * eigthNoteSamples;
    REQUIRE(g.loopLengthSamples(2) == Approx(expected).margin(0.5));
}

// ---- Quantize ---------------------------------------------------------------
TEST_CASE("Quantize Off returns unmodified position", "[loop][quantize]") {
    auto ts = TimeSignature::make44();
    auto g  = makeGrid(ts, 120.0);
    double pos = 12345.6;
    REQUIRE(LoopEngine::quantize(pos, g, QuantizeMode::Off) == Approx(pos));
}

TEST_CASE("Quantize MeasureStart snaps to nearest measure start", "[loop][quantize]") {
    auto ts = TimeSignature::make44();
    auto g  = makeGrid(ts, 120.0);
    double mDur = g.measureDurationSamples();
    // Position just past the start of measure 1 → should snap to that measure start
    double pos  = mDur + 100.0;
    double snapped = LoopEngine::quantize(pos, g, QuantizeMode::MeasureStart,
                                          QuantizeSnap::Nearest);
    REQUIRE(snapped == Approx(mDur).margin(0.5));
}

TEST_CASE("Quantize Next always returns a point after current position", "[loop][quantize]") {
    auto ts = TimeSignature::make44();
    auto g  = makeGrid(ts, 120.0);
    double pos = 1000.0;
    double snapped = LoopEngine::quantize(pos, g, QuantizeMode::SubBeat,
                                          QuantizeSnap::Next);
    REQUIRE(snapped > pos);
}

// ---- Long-loop drift (mathematical) ----------------------------------------
TEST_CASE("Loop boundary is exact: no accumulated drift after many cycles", "[loop][drift]") {
    // If loop start = 0 and loop length = L, after N cycles the position
    // (as computed by integer arithmetic) should still be exactly 0.
    auto ts = TimeSignature::make78({2,2,3});
    auto g  = makeGrid(ts, 140.0);
    double L = g.loopLengthSamples(1);

    // Simulate 10000 loop repetitions
    double pos = 0.0;
    for (int i = 0; i < 10000; ++i)
        pos = std::fmod(pos + L, L);

    REQUIRE(pos == Approx(0.0).margin(1.0));  // < 1 sample drift after 10k cycles
}

// ---- calculateMeasureLoop --------------------------------------------------
TEST_CASE("calculateMeasureLoop returns active loop region", "[loop]") {
    auto ts = TimeSignature::make44();
    auto g  = makeGrid(ts, 120.0);
    auto loop = LoopEngine::calculateMeasureLoop(0.0, 1, g,
                    QuantizeMode::MeasureStart, QuantizeSnap::Next);
    REQUIRE(loop.active);
    REQUIRE(loop.endSample > loop.startSample);
    REQUIRE(loop.endSample - loop.startSample ==
            Approx(g.loopLengthSamples(1)).margin(0.5));
}
