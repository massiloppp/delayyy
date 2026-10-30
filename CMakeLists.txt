cmake_minimum_required(VERSION 3.22)
project(ClassicDelay VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
if(JUCE_LOCAL_PATH)
  add_subdirectory(${JUCE_LOCAL_PATH} juce)
else()
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(ClassicDelay
  COMPANY_NAME "Massimo"
  PLUGIN_MANUFACTURER_CODE Msmo
  PLUGIN_CODE Cdly
  FORMATS VST3 Standalone
  PRODUCT_NAME "Classic Delay"
  IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
  VST3_CATEGORIES Fx Delay)

target_sources(ClassicDelay PRIVATE PluginProcessor.cpp PluginEditor.cpp)
target_compile_definitions(ClassicDelay PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(ClassicDelay PRIVATE juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
