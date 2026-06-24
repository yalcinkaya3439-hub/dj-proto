cmake_minimum_required(VERSION 3.19)

include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v2.13.10
)
FetchContent_MakeAvailable(Catch2)

add_executable(djcore_tests
    test_main.cpp
    test_waveform.cpp
    test_beatgrid.cpp
    test_loop.cpp
    test_data.cpp
)

target_link_libraries(djcore_tests
    PRIVATE
        djcore
        Catch2::Catch2
)

target_compile_features(djcore_tests PRIVATE cxx_std_20)

# Register the test binary directly — avoids catch_discover_tests portability issues
add_test(NAME djcore_all_tests COMMAND djcore_tests)
