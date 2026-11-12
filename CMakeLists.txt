cmake_minimum_required(VERSION 3.22)

# Mac: one build for Intel and Apple Silicon. Windows: no VC++ redistributable needed.
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(Undertow VERSION 2.0.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
if(MSVC)
    add_compile_options(/MP) # compile files in parallel
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    # JUCE 8.0.9, pinned to its exact commit so a moved tag can never change what gets built
    GIT_TAG f72bad64d29715216226685810c5196bd0d79d77)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Undertow
    COMPANY_NAME "FL Plugins"
    PLUGIN_MANUFACTURER_CODE Flpl
    PLUGIN_CODE Undt
    FORMATS VST3
    NEEDS_MIDI_INPUT TRUE
    PRODUCT_NAME "Undertow")

# The interface font, compiled into the plugin so it looks the same everywhere
juce_add_binary_data(UndertowAssets SOURCES
    Assets/SpaceGrotesk-Medium.ttf
    Assets/SpaceGrotesk-Bold.ttf)
set_target_properties(UndertowAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_sources(Undertow PRIVATE Source/PluginProcessor.cpp)
target_compile_definitions(Undertow PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Undertow PRIVATE
    UndertowAssets
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_recommended_config_flags)

enable_testing()
add_executable(duck_test Source/test_duck.cpp)
add_test(NAME duck_test COMMAND duck_test)

# Runs audio through the real processor, no host needed. Links the plugin's own
# static library so JUCE is compiled once, not once per target.
add_executable(processor_test Source/test_processor.cpp)
target_link_libraries(processor_test PRIVATE Undertow)
target_include_directories(processor_test PRIVATE $<TARGET_PROPERTY:Undertow,INCLUDE_DIRECTORIES>)
target_compile_definitions(processor_test PRIVATE $<TARGET_PROPERTY:Undertow,COMPILE_DEFINITIONS>)
add_test(NAME processor_test COMMAND processor_test)
