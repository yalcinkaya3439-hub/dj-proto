#include "djcore/TrackData.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <cstring>
#include <iomanip>

// Minimal JSON implementation — we use a very simple key/value approach
// with a hand-written serialiser to avoid external deps in the core lib.
// Production code should use nlohmann/json (already vendored via CMake
// FetchContent in the test build).

namespace djcore {

namespace fs = std::filesystem;

// ---- Helpers ----------------------------------------------------------------

static std::string escapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        if (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else out += c;
    }
    return out;
}

static std::string confidenceStr(BPMConfidence c) {
    switch(c) {
        case BPMConfidence::High:   return "high";
        case BPMConfidence::Medium: return "medium";
        default:                    return "low";
    }
}

static BPMConfidence parseConfidence(const std::string& s) {
    if (s == "high")   return BPMConfidence::High;
    if (s == "medium") return BPMConfidence::Medium;
    return BPMConfidence::Low;
}

// Very simple extraction of "key": value from JSON text (non-nested)
static std::string jsonStr(const std::string& json, const std::string& key) {
    std::string pat = "\"" + key + "\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    if (pos >= json.size()) return "";
    if (json[pos] == '"') {
        ++pos;
        std::string val;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) { ++pos; }
            val += json[pos++];
        }
        return val;
    }
    // number/bool/null
    std::string val;
    while (pos < json.size() && json[pos] != ',' && json[pos] != '}' && json[pos] != '\n')
        val += json[pos++];
    // trim
    while (!val.empty() && (val.back() == ' ' || val.back() == '\t')) val.pop_back();
    return val;
}

static double jsonDbl(const std::string& json, const std::string& key, double def = 0.0) {
    std::string s = jsonStr(json, key);
    if (s.empty()) return def;
    try { return std::stod(s); } catch(...) { return def; }
}

static int64_t jsonInt(const std::string& json, const std::string& key, int64_t def = 0) {
    std::string s = jsonStr(json, key);
    if (s.empty()) return def;
    try { return std::stoll(s); } catch(...) { return def; }
}

// Extract a JSON array of numbers
static std::vector<double> jsonDblArray(const std::string& json, const std::string& key) {
    std::string pat = "\"" + key + "\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos) return {};
    pos = json.find('[', pos);
    if (pos == std::string::npos) return {};
    ++pos;
    std::vector<double> result;
    while (pos < json.size() && json[pos] != ']') {
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == ',' || json[pos] == '\n')) ++pos;
        if (pos >= json.size() || json[pos] == ']') break;
        std::string num;
        while (pos < json.size() && json[pos] != ',' && json[pos] != ']' && json[pos] != ' ')
            num += json[pos++];
        try { result.push_back(std::stod(num)); } catch(...) {}
    }
    return result;
}

// ---- Fast file hash (SHA-256 of first 256 kB, manual djb2 as placeholder) --
// NOTE: For a production build replace with actual SHA-256 (e.g. OpenSSL or
// the JUCE SHA-256 class). This djb2 hash is fast and good enough for cache
// validation in the prototype.
std::string TrackDataStore::computeFileHash(const std::string& filePath) {
    std::ifstream f(filePath, std::ios::binary);
    if (!f) return "";
    const size_t READ_SIZE = 256 * 1024;
    std::vector<char> buf(READ_SIZE);
    f.read(buf.data(), READ_SIZE);
    size_t read = static_cast<size_t>(f.gcount());

    uint64_t hash = 5381;
    for (size_t i = 0; i < read; ++i)
        hash = ((hash << 5) + hash) ^ static_cast<uint8_t>(buf[i]);

    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return oss.str();
}

int64_t TrackDataStore::currentTimestamp() {
    return static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

// ---- TrackDataStore ---------------------------------------------------------

TrackDataStore::TrackDataStore(const std::string& dataDir)
    : dataDir_(dataDir)
{}

std::string TrackDataStore::dataPath(const std::string& filePath) const {
    if (dataDir_.empty()) {
        return filePath + ".djdata.json";
    }
    fs::path base = fs::path(filePath).filename();
    return (fs::path(dataDir_) / (base.string() + ".djdata.json")).string();
}

bool TrackDataStore::needsReanalysis(const std::string& filePath) {
    std::string path = dataPath(filePath);
    if (!fs::exists(path)) return true;
    std::ifstream f(path);
    std::string json((std::istreambuf_iterator<char>(f)), {});
    std::string storedHash = jsonStr(json, "fileHash");
    int64_t storedSize     = jsonInt(json, "fileSize");
    if (storedHash.empty()) return true;
    std::string currentHash = computeFileHash(filePath);
    int64_t currentSize = static_cast<int64_t>(fs::file_size(filePath));
    return (storedHash != currentHash || storedSize != currentSize);
}

std::optional<TrackAnalysis> TrackDataStore::load(const std::string& filePath) {
    std::string path = dataPath(filePath);
    if (!fs::exists(path)) return std::nullopt;

    std::ifstream f(path);
    if (!f) return std::nullopt;
    std::string json((std::istreambuf_iterator<char>(f)), {});

    // Verify hash
    std::string storedHash = jsonStr(json, "fileHash");
    std::string currentHash = computeFileHash(filePath);
    if (storedHash != currentHash) return std::nullopt;

    TrackAnalysis ta;
    ta.filePath        = filePath;
    ta.fileHash        = storedHash;
    ta.fileSize        = jsonInt(json, "fileSize");
    ta.modTime         = jsonInt(json, "modTime");
    ta.sampleRate      = static_cast<int>(jsonInt(json, "sampleRate", 44100));
    ta.totalSamples    = jsonInt(json, "totalSamples");
    ta.channels        = static_cast<int>(jsonInt(json, "channels", 1));
    ta.detectedBPM     = jsonDbl(json, "detectedBPM");
    ta.correctedBPM    = jsonDbl(json, "correctedBPM");
    ta.confidence      = parseConfidence(jsonStr(json, "confidence"));
    ta.confidenceScore = jsonDbl(json, "confidenceScore");
    ta.firstBeatSample = jsonDbl(json, "firstBeatSample");
    ta.timeSig.numerator   = static_cast<int>(jsonInt(json, "timeSigNum", 4));
    ta.timeSig.denominator = static_cast<int>(jsonInt(json, "timeSigDen", 4));
    std::string grpStr = jsonStr(json, "grouping");
    if (!grpStr.empty()) {
        try { ta.timeSig.grouping = TimeSignature::parseGrouping(grpStr); }
        catch(...) {}
    }
    ta.phraseStartSamples = jsonDblArray(json, "phraseStarts");
    ta.waveformCachePath  = jsonStr(json, "waveformCachePath");

    // Cue points (simplified — stored as flat arrays)
    // Full implementation would encode as a JSON array of objects
    return ta;
}

bool TrackDataStore::save(const TrackAnalysis& data) {
    if (!dataDir_.empty()) {
        fs::create_directories(dataDir_);
    }
    std::string path = dataPath(data.filePath);
    std::ofstream f(path);
    if (!f) return false;

    auto dbl = [](double v) {
        std::ostringstream o; o << std::setprecision(10) << v; return o.str();
    };

    f << "{\n";
    f << "  \"fileHash\": \""        << escapeJson(data.fileHash)    << "\",\n";
    f << "  \"fileSize\": "          << data.fileSize                << ",\n";
    f << "  \"modTime\": "           << data.modTime                 << ",\n";
    f << "  \"sampleRate\": "        << data.sampleRate              << ",\n";
    f << "  \"totalSamples\": "      << data.totalSamples            << ",\n";
    f << "  \"channels\": "          << data.channels                << ",\n";
    f << "  \"detectedBPM\": "       << dbl(data.detectedBPM)       << ",\n";
    f << "  \"correctedBPM\": "      << dbl(data.correctedBPM)      << ",\n";
    f << "  \"confidence\": \""      << confidenceStr(data.confidence) << "\",\n";
    f << "  \"confidenceScore\": "   << dbl(data.confidenceScore)   << ",\n";
    f << "  \"firstBeatSample\": "   << dbl(data.firstBeatSample)   << ",\n";
    f << "  \"timeSigNum\": "        << data.timeSig.numerator       << ",\n";
    f << "  \"timeSigDen\": "        << data.timeSig.denominator     << ",\n";
    f << "  \"grouping\": \""        << escapeJson(data.timeSig.groupingString()) << "\",\n";
    f << "  \"waveformCachePath\": \"" << escapeJson(data.waveformCachePath) << "\",\n";

    // Phrase starts array
    f << "  \"phraseStarts\": [";
    for (size_t i = 0; i < data.phraseStartSamples.size(); ++i) {
        if (i > 0) f << ", ";
        f << dbl(data.phraseStartSamples[i]);
    }
    f << "]\n";
    f << "}\n";
    return true;
}

} // namespace djcore
