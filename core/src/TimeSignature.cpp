#include "djcore/TimeSignature.h"
#include <sstream>
#include <numeric>
#include <stdexcept>
#include <cmath>

namespace djcore {

bool TimeSignature::isValid() const {
    if (numerator <= 0 || denominator <= 0) return false;
    if (grouping.empty()) return true; // ungrouped is still valid
    int sum = std::accumulate(grouping.begin(), grouping.end(), 0);
    return sum == numerator;
}

std::vector<int> TimeSignature::parseGrouping(const std::string& str) {
    std::vector<int> result;
    std::istringstream ss(str);
    std::string token;
    while (std::getline(ss, token, '+')) {
        if (token.empty()) throw std::invalid_argument("Empty token in grouping string");
        try {
            int v = std::stoi(token);
            if (v <= 0) throw std::invalid_argument("Grouping values must be positive");
            result.push_back(v);
        } catch (const std::exception&) {
            throw std::invalid_argument("Invalid grouping string: " + str);
        }
    }
    if (result.empty()) throw std::invalid_argument("Empty grouping string");
    return result;
}

std::string TimeSignature::groupingString() const {
    if (grouping.empty()) return "";
    std::string out;
    for (size_t i = 0; i < grouping.size(); ++i) {
        if (i > 0) out += '+';
        out += std::to_string(grouping[i]);
    }
    return out;
}

bool TimeSignature::isGroupingValid(const std::vector<int>& g) const {
    if (g.empty()) return false;
    int sum = std::accumulate(g.begin(), g.end(), 0);
    return sum == numerator;
}

// BPM counts denominator-notes per minute.
// sub-beat duration = 60.0 / BPM seconds
double TimeSignature::subBeatDurationSeconds(double bpm) const {
    return 60.0 / bpm;
}

double TimeSignature::measureDurationSeconds(double bpm) const {
    return static_cast<double>(numerator) * subBeatDurationSeconds(bpm);
}

double TimeSignature::subBeatDurationSamples(double bpm, int sampleRate) const {
    return subBeatDurationSeconds(bpm) * static_cast<double>(sampleRate);
}

double TimeSignature::measureDurationSamples(double bpm, int sampleRate) const {
    return measureDurationSeconds(bpm) * static_cast<double>(sampleRate);
}

// ---- Preset factories ----

TimeSignature TimeSignature::make24() { return {2, 4, {}}; }
TimeSignature TimeSignature::make34() { return {3, 4, {}}; }
TimeSignature TimeSignature::make44() { return {4, 4, {}}; }
TimeSignature TimeSignature::make58() { return {5, 8, {2,3}}; }
TimeSignature TimeSignature::make68() { return {6, 8, {3,3}}; }
TimeSignature TimeSignature::make78(std::vector<int> g) {
    if (g.empty()) g = {2,2,3};
    return {7, 8, g};
}
TimeSignature TimeSignature::make88() { return {8, 8, {4,4}}; }
TimeSignature TimeSignature::make98(std::vector<int> g) {
    if (g.empty()) g = {2,2,2,3};
    return {9, 8, g};
}
TimeSignature TimeSignature::make108() { return {10, 8, {3,3,4}}; }
TimeSignature TimeSignature::make128() { return {12, 8, {3,3,3,3}}; }

} // namespace djcore
