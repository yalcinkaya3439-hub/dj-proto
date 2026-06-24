cmake_minimum_required(VERSION 3.19)

juce_add_gui_app(DJProtoApp
    PRODUCT_NAME        "DJ Proto"
    VERSION             "0.1.0"
    COMPANY_NAME        "DJProto"
    BUNDLE_ID           "com.djproto.app"
    NEEDS_CURL          FALSE
    NEEDS_WEB_BROWSER   FALSE
)

juce_generate_juce_header(DJProtoApp)

target_sources(DJProtoApp PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/DeckComponent.cpp
    Source/WaveformDisplay.cpp
    Source/AudioEngine.cpp
)

target_compile_features(DJProtoApp PRIVATE cxx_std_20)

target_link_libraries(DJProtoApp
    PRIVATE
        djcore
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_binary_data(AppBinaryData SOURCES
    # Add any resource files here (icons, etc.)
)
