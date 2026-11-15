cmake_minimum_required(VERSION 3.22)
project(VocalMetronome VERSION 1.0.0 LANGUAGES C CXX)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.6 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)
juce_add_binary_data(VocalMetronomeAudio SOURCES Assets/Count1.wav Assets/Count2.wav Assets/Count3.wav Assets/Count4.wav)
juce_add_plugin(VocalMetronome
    COMPANY_NAME "VocalMetronome" BUNDLE_ID "audio.vocalmetronome.plugin"
    PLUGIN_MANUFACTURER_CODE Vcmt PLUGIN_CODE Vcm1
    FORMATS VST3 PRODUCT_NAME "Vocal-Metronome"
    IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth)
juce_generate_juce_header(VocalMetronome)
target_sources(VocalMetronome PRIVATE Source/Plugin.cpp)
target_compile_features(VocalMetronome PRIVATE cxx_std_17)
target_compile_definitions(VocalMetronome PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(VocalMetronome PRIVATE VocalMetronomeAudio juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
