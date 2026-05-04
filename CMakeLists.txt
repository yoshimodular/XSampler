cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "" FORCE)
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")

project(XSampler VERSION 0.0.6 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

# ---------- JUCE ----------
FetchContent_Declare(
    JUCE
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.4.zip
)
FetchContent_MakeAvailable(JUCE)

# ---------- sfizz ----------
set(SFIZZ_JACK        OFF CACHE BOOL "" FORCE)
set(SFIZZ_RENDER      OFF CACHE BOOL "" FORCE)
set(SFIZZ_LV2         OFF CACHE BOOL "" FORCE)
set(SFIZZ_VST         OFF CACHE BOOL "" FORCE)
set(SFIZZ_AU          OFF CACHE BOOL "" FORCE)
set(SFIZZ_SHARED      OFF CACHE BOOL "" FORCE)
set(SFIZZ_USE_SNDFILE OFF CACHE BOOL "" FORCE)
set(SFIZZ_USE_VCPKG   OFF CACHE BOOL "" FORCE)
set(SFIZZ_ENABLE_LTO  OFF CACHE BOOL "" FORCE)
set(SFIZZ_LOGO        OFF CACHE BOOL "" FORCE)
set(SFIZZ_TESTS       OFF CACHE BOOL "" FORCE)
set(SFIZZ_DEMOS       OFF CACHE BOOL "" FORCE)
set(SFIZZ_DEVTOOLS    OFF CACHE BOOL "" FORCE)
set(SFIZZ_BENCHMARKS  OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    sfizz
    GIT_REPOSITORY https://github.com/sfztools/sfizz.git
    GIT_TAG        1.2.3
    GIT_SUBMODULES_RECURSE TRUE
    GIT_SHALLOW    TRUE
    # Strip ARM32-only flags (-mfpu=neon, -mfloat-abi=hard) that sfizz applies
    # for any "arm.*" CPU — invalid for Apple Silicon (arm64).
    PATCH_COMMAND  sh -c "sed -i '' -e 's/-mfpu=neon//g' -e 's/-mfloat-abi=hard//g' cmake/SfizzConfig.cmake; sed -i '' -e 's/Base::template do_pop_any/Base::do_pop_any/g' -e 's/Base::template do_push_any/Base::do_push_any/g' external/atomic_queue/include/atomic_queue/atomic_queue.h || true"
)
FetchContent_MakeAvailable(sfizz)

# ---------- Plugin ----------
juce_add_plugin(XSampler
    VERSION                  ${PROJECT_VERSION}
    PLUGIN_NAME              "XSampler"
    COMPANY_NAME             "XSampler"
    PLUGIN_MANUFACTURER_CODE Xsam
    PLUGIN_CODE              Xsmp
    BUNDLE_ID                "com.xsampler.XSampler"
    FORMATS                  VST3 AU Standalone
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_AU_MAIN_TYPE      kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD  FALSE
)

juce_generate_juce_header(XSampler)

target_sources(XSampler PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/SfzOverride.cpp
    Source/SfzOverride.h
    Source/Arpeggiator.cpp
    Source/Arpeggiator.h
)

target_compile_definitions(XSampler PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(XSampler PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    sfizz::sfizz
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

target_compile_options(XSampler PRIVATE -Wall -Wno-unused-parameter)

# ---------- Tests ----------
option(XSAMPLER_BUILD_TESTS "Build XSampler unit tests" ON)

if (XSAMPLER_BUILD_TESTS)
    juce_add_console_app(XSamplerTests
        PRODUCT_NAME "XSamplerTests"
    )
    juce_generate_juce_header(XSamplerTests)

    target_sources(XSamplerTests PRIVATE
        Tests/Tests.cpp
    )

    target_compile_definitions(XSamplerTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_UNIT_TESTS=1
        XSAMPLER_TESTING=1
    )

    target_link_libraries(XSamplerTests PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        sfizz::sfizz
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
    )

    target_compile_options(XSamplerTests PRIVATE -Wall -Wno-unused-parameter)
endif()
