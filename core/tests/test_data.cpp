#include <catch2/catch.hpp>
#include "djcore/TrackData.h"
#include "djcore/TimeSignature.h"
#include <filesystem>
#include <fstream>

using namespace djcore;
namespace fs = std::filesystem;

static TrackAnalysis makeSampleAnalysis(const std::string& path) {
    TrackAnalysis ta;
    ta.filePath         = path;
    ta.fileHash         = "deadbeef00000000";
    ta.fileSize         = 1024 * 1024;
    ta.modTime          = 1700000000;
    ta.sampleRate       = 44100;
    ta.totalSamples     = 44100 * 60;
    ta.channels         = 2;
    ta.detectedBPM      = 140.0;
    ta.correctedBPM     = 0.0;
    ta.confidence       = BPMConfidence::High;
    ta.confidenceScore  = 0.82;
    ta.firstBeatSample  = 512.0;
    ta.timeSig          = TimeSignature::make78({2,2,3});
    ta.phraseStartSamples = {0.0, 44100.0 * 16 * 7.0 / 8.0};
    return ta;
}

TEST_CASE("Save and load TrackAnalysis round-trips cleanly", "[trackdata]") {
    // Use a temp directory
    fs::path tmpDir = fs::temp_directory_path() / "djcore_test";
    fs::create_directories(tmpDir);

    std::string fakeAudio = (tmpDir / "test_track.wav").string();
    // Create a dummy file so hash can be computed
    {
        std::ofstream f(fakeAudio, std::ios::binary);
        std::vector<char> dummy(64 * 1024, 0x42);
        f.write(dummy.data(), dummy.size());
    }

    TrackDataStore store(tmpDir.string());

    // Compute hash and set it in the analysis
    auto ta = makeSampleAnalysis(fakeAudio);
    ta.fileHash = TrackDataStore::computeFileHash(fakeAudio);
    ta.fileSize = static_cast<int64_t>(fs::file_size(fakeAudio));

    REQUIRE(store.save(ta));

    auto loaded = store.load(fakeAudio);
    REQUIRE(loaded.has_value());

    REQUIRE(loaded->detectedBPM     == Approx(140.0));
    REQUIRE(loaded->firstBeatSample == Approx(512.0));
    REQUIRE(loaded->timeSig.numerator   == 7);
    REQUIRE(loaded->timeSig.denominator == 8);
    REQUIRE(loaded->timeSig.groupingString() == "2+2+3");
    REQUIRE(loaded->confidence == BPMConfidence::High);
    REQUIRE(loaded->phraseStartSamples.size() == 2);

    fs::remove_all(tmpDir);
}

TEST_CASE("Load returns nullopt when file does not exist", "[trackdata]") {
    fs::path tmpDir = fs::temp_directory_path() / "djcore_test2";
    fs::create_directories(tmpDir);
    TrackDataStore store(tmpDir.string());
    auto result = store.load("/nonexistent/track.wav");
    REQUIRE_FALSE(result.has_value());
    fs::remove_all(tmpDir);
}

TEST_CASE("needsReanalysis returns true when file changes", "[trackdata]") {
    fs::path tmpDir = fs::temp_directory_path() / "djcore_test3";
    fs::create_directories(tmpDir);

    std::string fakeAudio = (tmpDir / "track.wav").string();
    {
        std::ofstream f(fakeAudio, std::ios::binary);
        std::vector<char> d(64 * 1024, 0x11);
        f.write(d.data(), d.size());
    }

    TrackDataStore store(tmpDir.string());
    auto ta    = makeSampleAnalysis(fakeAudio);
    ta.fileHash = TrackDataStore::computeFileHash(fakeAudio);
    ta.fileSize = static_cast<int64_t>(fs::file_size(fakeAudio));
    store.save(ta);

    // Modify the file
    {
        std::ofstream f(fakeAudio, std::ios::binary | std::ios::app);
        std::vector<char> extra(4096, 0xFF);
        f.write(extra.data(), extra.size());
    }

    REQUIRE(store.needsReanalysis(fakeAudio));
    fs::remove_all(tmpDir);
}

TEST_CASE("effectiveBPM returns correctedBPM when set, else detectedBPM", "[trackdata]") {
    TrackAnalysis ta;
    ta.detectedBPM  = 128.0;
    ta.correctedBPM = 0.0;
    REQUIRE(ta.effectiveBPM() == Approx(128.0));

    ta.correctedBPM = 127.5;
    REQUIRE(ta.effectiveBPM() == Approx(127.5));
}
