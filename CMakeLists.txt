cmake_minimum_required(VERSION 3.22)

# Opciones de plataforma (deben ir antes de project())
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Version minima de macOS")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Binario universal")

project(JellyTune VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# En Windows: enlaza el runtime de C++ estaticamente para que el plugin
# funcione en cualquier PC sin instalar "Visual C++ Redistributable".
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Descarga JUCE automaticamente al configurar (requiere internet y git)
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(JellyTune
    COMPANY_NAME "Jelly"
    PLUGIN_MANUFACTURER_CODE Jlly
    PLUGIN_CODE Jtun
    FORMATS VST3
    PRODUCT_NAME "JellyTune"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES "Fx" "Pitch Shift"
    COPY_PLUGIN_AFTER_BUILD FALSE)

# El logo se incrusta dentro del plugin
juce_add_binary_data(JellyTuneAssets SOURCES Resources/logo.png)

target_sources(JellyTune PRIVATE
    Source/JellyTuneDSP.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h)

target_compile_definitions(JellyTune PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(JellyTune
    PRIVATE
        JellyTuneAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
