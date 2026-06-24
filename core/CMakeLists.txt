cmake_minimum_required(VERSION 3.19)

add_library(djcore STATIC
    src/TimeSignature.cpp
    src/WaveformAnalyzer.cpp
    src/BPMAnalyzer.cpp
    src/BeatGrid.cpp
    src/LoopEngine.cpp
    src/TrackData.cpp
)

target_include_directories(djcore PUBLIC include)
target_compile_features(djcore PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(djcore PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(DJCORE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
