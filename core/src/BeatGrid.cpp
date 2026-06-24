#include "djcore/BeatGrid.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace djcore {

BeatGrid::BeatGrid(double bpm, double firstBeatSample,
                   const TimeSignature& timeSig, int sampleRate)
    : bpm_(bpm), firstBeatSample_(firstBeatSample),
      timeSig_(timeSig), sampleRate_(sampleRate)
{}

double BeatGrid::subBeatDurationSamples() const {
    return timeSig_.subBeatDurationSamples(bpm_, sampleRate_);
}

double BeatGrid::measureDurationSamples() const {
    return timeSig_.measureDurationSamples(bpm_, sampleRate_);
}

double BeatGrid::loopLengthSamples(int measures) const {
    return measures * measureDurationSamples();
}

void BeatGrid::recompute(int64_t totalSamples) {
    marks_.clear();
    if (bpm_ <= 0.0 || totalSamples <= 0) return;

    double sbDur = subBeatDurationSamples();
    if (sbDur <= 0.0) return;

    // Build grouping lookup: for sub-beat index i within a measure,
    // what group does it belong to, and is it a group start?
    const auto& grp = timeSig_.grouping;
    int numSubBeats = timeSig_.numerator;

    // sub-beat → group index (0-based)
    std::vector<int>  groupOf(numSubBeats, 0);
    std::vector<bool> isGroupStart(numSubBeats, false);
    if (!grp.empty()) {
        int sb = 0;
        for (int g = 0; g < static_cast<int>(grp.size()); ++g) {
            isGroupStart[sb] = true;
            for (int k = 0; k < grp[g]; ++k, ++sb) {
                groupOf[sb] = g;
            }
        }
    } else {
        // No explicit grouping: treat every sub-beat as a group start
        for (int i = 0; i < numSubBeats; ++i) {
            isGroupStart[i] = true;
            groupOf[i]      = i;
        }
    }

    // Build phrase start lookup
    std::vector<double> sortedPhrases = phraseStarts_;
    std::sort(sortedPhrases.begin(), sortedPhrases.end());

    // Walk from firstBeatSample_ backward to find the earliest mark position
    // that is >= 0 and still within totalSamples.
    // We generate marks starting from the measure containing sample 0.

    double measureDur = measureDurationSamples();
    double mStart     = firstBeatSample_;
    // Wind back to measure containing sample 0
    while (mStart > 0.0) mStart -= measureDur;
    mStart += measureDur;  // first measure starting at or after 0
    // Actually we want the first measure that has any portion >= 0
    mStart -= measureDur;  // go one back to capture partial measures at start

    int measureIdx = 0;
    for (double mPos = mStart; mPos < static_cast<double>(totalSamples); mPos += measureDur, ++measureIdx) {
        for (int sb = 0; sb < numSubBeats; ++sb) {
            double pos = mPos + sb * sbDur;
            if (pos < 0.0) continue;
            if (pos >= static_cast<double>(totalSamples)) goto done;

            BeatMark mark;
            mark.samplePosition = pos;
            mark.subBeatIndex   = sb;
            mark.groupIndex     = groupOf[sb];
            mark.measureIndex   = measureIdx;

            if (sb == 0) {
                // Check if it is a phrase start
                bool isPhraseStart = std::binary_search(sortedPhrases.begin(), sortedPhrases.end(), pos);
                mark.level = isPhraseStart ? BeatLevel::PhraseStart : BeatLevel::MeasureStart;
            } else if (!grp.empty() && isGroupStart[sb]) {
                mark.level = BeatLevel::GroupStart;
            } else {
                mark.level = BeatLevel::SubBeat;
            }

            marks_.push_back(mark);
        }
    }
    done:;

    // Also tag any phrase start samples that weren't already on a measure start
    for (double ps : sortedPhrases) {
        auto it = std::find_if(marks_.begin(), marks_.end(),
            [ps](const BeatMark& m) { return std::abs(m.samplePosition - ps) < 1.0; });
        if (it != marks_.end())
            it->level = BeatLevel::PhraseStart;
    }
}

std::vector<BeatMark> BeatGrid::getMarksInRange(double startSample, double endSample) const {
    std::vector<BeatMark> result;
    for (const auto& m : marks_) {
        if (m.samplePosition >= startSample && m.samplePosition < endSample)
            result.push_back(m);
    }
    return result;
}

BeatMark BeatGrid::nearestAtOrBefore(double pos, BeatLevel minLevel) const {
    BeatMark best;
    best.samplePosition = 0.0;
    bool found = false;
    for (const auto& m : marks_) {
        if (m.level < minLevel) continue;
        if (m.samplePosition <= pos) {
            if (!found || m.samplePosition > best.samplePosition) {
                best  = m;
                found = true;
            }
        }
    }
    return best;
}

BeatMark BeatGrid::nextAfter(double pos, BeatLevel minLevel) const {
    BeatMark best;
    best.samplePosition = std::numeric_limits<double>::max();
    bool found = false;
    for (const auto& m : marks_) {
        if (m.level < minLevel) continue;
        if (m.samplePosition > pos) {
            if (!found || m.samplePosition < best.samplePosition) {
                best  = m;
                found = true;
            }
        }
    }
    return best;
}

void BeatGrid::setBPM(double bpm) { bpm_ = bpm; }
void BeatGrid::setFirstBeatSample(double s) { firstBeatSample_ = s; }
void BeatGrid::setTimeSig(const TimeSignature& ts) { timeSig_ = ts; }

void BeatGrid::shiftGrid(double deltaSamples) {
    firstBeatSample_ += deltaSamples;
    for (auto& m : marks_) m.samplePosition += deltaSamples;
}

void BeatGrid::addAnchor(const GridAnchor& anchor) {
    anchors_.push_back(anchor);
    std::sort(anchors_.begin(), anchors_.end(),
        [](const GridAnchor& a, const GridAnchor& b) {
            return a.samplePosition < b.samplePosition;
        });
}

void BeatGrid::clearAnchors() { anchors_.clear(); }

void BeatGrid::setPhraseStarts(const std::vector<double>& samples) {
    phraseStarts_ = samples;
}

} // namespace djcore
