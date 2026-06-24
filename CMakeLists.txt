cmake_minimum_required(VERSION 3.19)
project(DJProto VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Options ----------------------------------------------------------------
option(DJCORE_BUILD_TESTS "Build platform-independent unit tests" ON)
option(DJAPP_BUILD        "Build JUCE Windows application"        OFF)

# ---- CTest must be enabled before subdirectories that define tests ----------
if(DJCORE_BUILD_TESTS)
    enable_testing()
endif()

# ---- Core library (always built) -------------------------------------------
add_subdirectory(core)

# ---- JUCE application (Windows / macOS only) --------------------------------
if(DJAPP_BUILD)
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        7.0.12
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)
    add_subdirectory(app)
endif()
