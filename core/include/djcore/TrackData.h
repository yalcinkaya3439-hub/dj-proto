#pragma once
#include "BeatGrid.h"
#include "BPMAnalyzer.h"
#include "TimeSignature.h"
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace djcore {

struct CuePoint {
    std::string id;
    double      samplePosition = 0.0;
    std::string type;    // "normal" | "load" | "loop" | "grid" | "phrase"
    std::string name;
    std::string color;
    double      loopLengthSamples = 0.0;  // 0 = no loop
    bool        quantized = false;
};

struct TrackAnalysis {
    // ---- File identity ----
    std::string filePath;
    std::string fileHash;    // SHA-256 hex, first 64 kB of file for speed
    int64_t     fileSize  = 0;
    int64_t     modTime   = 0;  // Unix timestamp

    // ---- Audio properties ----
    int     sampleRate   = 44100;
    int64_t totalSamples = 0;   // per channel
    int     channels     = 1;

    // ---- BPM / grid ----
    double         detectedBPM    = 0.0;
    double         correctedBPM   = 0.0;  // 0 = use detectedBPM
    BPMConfidence  confidence     = BPMConfidence::Low;
    double         confidenceScore = 0.0;
    double         firstBeatSample = 0.0;

    TimeSignature  timeSig;
    std::vector<GridAnchor> gridAnchors;

    // ---- Musical markers ----
    std::vector<double>    phraseStartSamples;
    std::vector<CuePoint>  cuePoints;

    // ---- Waveform cache ----
    std::string waveformCachePath;  // path to .wfc binary file (peak data)

    // ---- Helpers ----
    double effectiveBPM() const {
        return correctedBPM > 0.0 ? correctedBPM : detectedBPM;
    }
};

// JSON-based per-song data store.
// Data is saved as <audioFilePath>.djdata.json next to the audio file
// OR in a user-specified data directory.
class TrackDataStore {
public:
    // dataDir: if empty, saves next to audio file; otherwise saves in dataDir/
    explicit TrackDataStore(const std::string& dataDir = "");

    // Load analysis for a file. Returns nullopt if not found or hash mismatch.
    std::optional<TrackAnalysis> load(const std::string& filePath);

    // Save/update analysis for a file.
    bool save(const TrackAnalysis& data);

    // Invalidate cache if file changed (hash or size mismatch).
    bool needsReanalysis(const std::string& filePath);

    // Compute a fast hash of the file (SHA-256 of first 256 kB)
    static std::string computeFileHash(const std::string& filePath);

    // Current Unix timestamp
    static int64_t currentTimestamp();

private:
    std::string dataDir_;
    std::string dataPath(const std::string& filePath) const;
};

} // namespace djcore
