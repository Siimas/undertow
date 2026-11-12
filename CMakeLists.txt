cmake_minimum_required(VERSION 3.22)

# Mac: one build for Intel and Apple Silicon. Windows: no VC++ redistributable needed.
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(Pump VERSION 1.2.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
if(MSVC)
    add_compile_options(/MP) # compile files in parallel
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.9
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Pump
    COMPANY_NAME "FL Plugins"
    PLUGIN_MANUFACTURER_CODE Flpl
    PLUGIN_CODE Pump
    FORMATS VST3
    NEEDS_MIDI_INPUT TRUE
    PRODUCT_NAME "Pump")

target_sources(Pump PRIVATE Source/PluginProcessor.cpp)
target_compile_definitions(Pump PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Pump PRIVATE
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_recommended_config_flags)

enable_testing()
add_executable(duck_test Source/test_duck.cpp)
add_test(NAME duck_test COMMAND duck_test)

# Runs audio through the real processor, no host needed. Links the plugin's own
# static library so JUCE is compiled once, not once per target.
add_executable(processor_test Source/test_processor.cpp)
target_link_libraries(processor_test PRIVATE Pump)
target_include_directories(processor_test PRIVATE $<TARGET_PROPERTY:Pump,INCLUDE_DIRECTORIES>)
target_compile_definitions(processor_test PRIVATE $<TARGET_PROPERTY:Pump,COMPILE_DEFINITIONS>)
add_test(NAME processor_test COMMAND processor_test)
