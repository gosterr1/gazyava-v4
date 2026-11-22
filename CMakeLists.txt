cmake_minimum_required(VERSION 3.22)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(Gazyava VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 7.0.12 GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_binary_data(GazAssets SOURCES
    Assets/can_big.png Assets/can_green.png Assets/can_baja.png Assets/can_purple.png Assets/can_liberty.png Assets/can_black.png
    Assets/lid.png Assets/speaker.png Assets/manul_left.jpg Assets/manul_right.jpg
    Assets/meow_a.wav Assets/meow_b.wav Assets/meow_c.wav
    Assets/font_title.ttf Assets/font_ui.ttf)

juce_add_plugin(Gazyava
    COMPANY_NAME "Gazyava"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Gzya PLUGIN_CODE Gz01
    FORMATS VST3 Standalone
    PRODUCT_NAME "Gazyava"
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(Gazyava)
target_sources(Gazyava PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(Gazyava PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
if(MSVC)
    target_compile_options(Gazyava PRIVATE /utf-8)
endif()
target_link_libraries(Gazyava PRIVATE GazAssets juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
