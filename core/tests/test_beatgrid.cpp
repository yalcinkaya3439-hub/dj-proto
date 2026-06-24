#include <catch2/catch.hpp>
#include "djcore/BeatGrid.h"
#include "djcore/TimeSignature.h"
#include <cmath>
#include <vector>
#include <limits>

using namespace djcore;

static constexpr int SR = 44100;

// Helper: duration of one sub-beat in samples for a given BPM
static double sbDur(double bpm) { return 60.0 / bpm * SR; }

// ---------------------------------------------------------------------------
// 4/4 sanity checks
// ---------------------------------------------------------------------------
TEST_CASE("4/4 at 120 BPM: measure duration equals 4 quarter notes", "[beatgrid][4/4]") {
    auto ts = TimeSignature::make44();
    BeatGrid g(120.0, 0.0, ts, SR);
    // quarter-note = 60/120 = 0.5 s = 22050 samples
    // measure = 4 * 22050 = 88200 samples
    REQUIRE(g.measureDurationSamples() == Approx(88200.0).margin(0.5));
}

TEST_CASE("4/4 marks: first mark is MeasureStart at firstBeatSample", "[beatgrid][4/4]") {
    auto ts = TimeSignature::make44();
    BeatGrid g(120.0, 0.0, ts, SR);
    g.recompute(SR * 10);
    auto marks = g.getMarksInRange(0.0, 1.0);  // just near time 0
    REQUIRE(!marks.empty());
    REQUIRE(marks.front().level == BeatLevel::MeasureStart);
    REQUIRE(marks.front().samplePosition == Approx(0.0).margin(0.5));
}

// ---------------------------------------------------------------------------
// 7/8 tests
// ---------------------------------------------------------------------------
TEST_CASE("7/8 at 140 BPM (eighth-note BPM): measure = 7 eighth-notes", "[beatgrid][7/8]") {
    // eighth-note duration = 60/140 = 0.4286 s = 18900 samples
    // measure = 7 * 18900 = 132300 samples
    auto ts = TimeSignature::make78({2,2,3});
    BeatGrid g(140.0, 0.0, ts, SR);
    double expectedMeasure = 7.0 * 60.0 / 140.0 * SR;
    REQUIRE(g.measureDurationSamples() == Approx(expectedMeasure).margin(0.5));
}

TEST_CASE("7/8 2+2+3: group starts at sub-beats 0, 2, 4", "[beatgrid][7/8]") {
    auto ts = TimeSignature::make78({2,2,3});
    BeatGrid g(120.0, 0.0, ts, SR);
    g.recompute(SR * 10);

    // First measure marks in range [0, measureDur)
    double mDur = g.measureDurationSamples();
    auto marks = g.getMarksInRange(0.0, mDur);

    // Collect sub-beat indices of GroupStart marks (non-MeasureStart)
    std::vector<int> groupStarts;
    for (auto& m : marks) {
        if (m.level == BeatLevel::GroupStart)
            groupStarts.push_back(m.subBeatIndex);
    }
    // MeasureStart at sub-beat 0 is already BeatLevel::MeasureStart
    // GroupStart at sub-beat 2 (after "2") and sub-beat 4 (after "2+2")
    REQUIRE(std::find(groupStarts.begin(), groupStarts.end(), 2) != groupStarts.end());
    REQUIRE(std::find(groupStarts.begin(), groupStarts.end(), 4) != groupStarts.end());
}

TEST_CASE("7/8 2+3+2: group starts at sub-beats 0, 2, 5", "[beatgrid][7/8]") {
    auto ts = TimeSignature::make78({2,3,2});
    BeatGrid g(120.0, 0.0, ts, SR);
    g.recompute(SR * 10);
    double mDur = g.measureDurationSamples();
    auto marks = g.getMarksInRange(0.0, mDur);

    std::vector<int> groupStarts;
    for (auto& m : marks)
        if (m.level == BeatLevel::GroupStart)
            groupStarts.push_back(m.subBeatIndex);

    REQUIRE(std::find(groupStarts.begin(), groupStarts.end(), 2) != groupStarts.end());
    REQUIRE(std::find(groupStarts.begin(), groupStarts.end(), 5) != groupStarts.end());
}

TEST_CASE("7/8 3+2+2: group starts at sub-beats 0, 3, 5", "[beatgrid][7/8]") {
    auto ts = TimeSignature::make78({3,2,2});
    BeatGrid g(120.0, 0.0, ts, SR);
    g.recompute(SR * 10);
    double mDur = g.measureDurationSamples();
    auto marks = g.getMarksInRange(0.0, mDur);

    std::vector<int> groupStarts;
    for (auto& m : marks)
        if (m.level == BeatLevel::GroupStart)
            groupStarts.push_back(m.subBeatIndex);

    REQUIRE(std::find(groupStarts.begin(), groupStarts.end(), 3) != groupStarts.end());
    REQUIRE(std::find(groupStarts.begin(), groupStarts.end(), 5) != groupStarts.end());
}

// ---------------------------------------------------------------------------
// 9/8 tests
// ---------------------------------------------------------------------------
TEST_CASE("9/8 at 120 BPM: measure = 9 eighth-notes", "[beatgrid][9/8]") {
    auto ts = TimeSignature::make98({2,2,2,3});
    BeatGrid g(120.0, 0.0, ts, SR);
    double expectedMeasure = 9.0 * 60.0 / 120.0 * SR;
    REQUIRE(g.measureDurationSamples() == Approx(expectedMeasure).margin(0.5));
}

TEST_CASE("9/8 2+2+2+3: group starts at sub-beats 0, 2, 4, 6", "[beatgrid][9/8]") {
    auto ts = TimeSignature::make98({2,2,2,3});
    BeatGrid g(120.0, 0.0, ts, SR);
    g.recompute(SR * 10);
    double mDur = g.measureDurationSamples();
    auto marks = g.getMarksInRange(0.0, mDur);

    std::vector<int> groupStarts;
    for (auto& m : marks)
        if (m.level == BeatLevel::GroupStart)
            groupStarts.push_back(m.subBeatIndex);

    REQUIRE(std::find(groupStarts.begin(), groupStarts.end(), 2) != groupStarts.end());
    REQUIRE(std::find(groupStarts.begin(), groupStarts.end(), 4) != groupStarts.end());
    REQUIRE(std::find(groupStarts.begin(), groupStarts.end(), 6) != groupStarts.end());
}

TEST_CASE("9/8 3+2+2+2: group starts at sub-beats 0, 3, 5, 7", "[beatgrid][9/8]") {
    auto ts = TimeSignature::make98({3,2,2,2});
    BeatGrid g(120.0, 0.0, ts, SR);
    g.recompute(SR * 10);
    double mDur = g.measureDurationSamples();
    auto marks = g.getMarksInRange(0.0, mDur);

    std::vector<int> groupStarts;
    for (auto& m : marks)
        if (m.level == BeatLevel::GroupStart)
            groupStarts.push_back(m.subBeatIndex);

    REQUIRE(std::find(groupStarts.begin(), groupStarts.end(), 3) != groupStarts.end());
    REQUIRE(std::find(groupStarts.begin(), groupStarts.end(), 5) != groupStarts.end());
    REQUIRE(std::find(groupStarts.begin(), groupStarts.end(), 7) != groupStarts.end());
}

// ---------------------------------------------------------------------------
// Grid shift
// ---------------------------------------------------------------------------
TEST_CASE("shiftGrid moves all marks by delta", "[beatgrid]") {
    auto ts = TimeSignature::make44();
    BeatGrid g(120.0, 0.0, ts, SR);
    g.recompute(SR * 4);

    double delta = 500.0;
    auto marksBefore = g.getMarks();
    g.shiftGrid(delta);
    auto marksAfter = g.getMarks();

    REQUIRE(marksBefore.size() == marksAfter.size());
    for (size_t i = 0; i < marksBefore.size(); ++i)
        REQUIRE(marksAfter[i].samplePosition ==
                Approx(marksBefore[i].samplePosition + delta).margin(0.5));
}

// ---------------------------------------------------------------------------
// Grouping validation
// ---------------------------------------------------------------------------
TEST_CASE("TimeSignature grouping validation", "[timesig]") {
    auto ts = TimeSignature::make78({2,2,3});
    REQUIRE(ts.isValid());

    auto bad = TimeSignature::make78({2,2,2});  // sums to 6, not 7
    REQUIRE_FALSE(bad.isValid());
}

TEST_CASE("parseGrouping parses correctly", "[timesig]") {
    auto g = TimeSignature::parseGrouping("2+2+3");
    REQUIRE(g.size() == 3);
    REQUIRE(g[0] == 2);
    REQUIRE(g[1] == 2);
    REQUIRE(g[2] == 3);
}

TEST_CASE("parseGrouping throws on bad input", "[timesig]") {
    REQUIRE_THROWS_AS(TimeSignature::parseGrouping(""), std::invalid_argument);
    REQUIRE_THROWS_AS(TimeSignature::parseGrouping("2++3"), std::invalid_argument);
}
