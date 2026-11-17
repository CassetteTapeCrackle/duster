cmake_minimum_required(VERSION 3.25)
project(Duster VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# 12.0: the WebView UI (JUCE's frontend uses private class methods) needs WebKit from Safari 15.
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")

option(DUSTER_BUILD_PLUGIN "Build the AU/VST3 plugin" ON)
option(DUSTER_BUILD_TESTS "Build Catch2 unit tests" ON)
option(DUSTER_INSTALL_PLUGIN "Copy the built plugin into ~/Library/Audio/Plug-Ins" OFF)
option(DUSTER_SANITIZE "ASan + UBSan on everything (test builds only, never load in a host)" OFF)
option(DUSTER_WERROR "Treat warnings in our own sources as errors (CI); JUCE's module files are exempt" OFF)

if(DUSTER_SANITIZE AND DUSTER_BUILD_PLUGIN)
    message(FATAL_ERROR "DUSTER_SANITIZE is for test-only builds: set DUSTER_BUILD_PLUGIN=OFF (use the asan preset)")
endif()
if(DUSTER_SANITIZE)
    # -fno-sanitize-recover makes UBSan findings fail the test run instead of just printing.
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined -fno-sanitize-recover=all)
endif()

enable_testing()
include(cmake/CPM.cmake)

# ---- Core: pure C++ DSP and engine at 48 kHz, no JUCE. Used by the plugin and the tests. ----
set(DUSTER_CORE_SOURCES
    Source/Engine/KickOscillator.cpp
    Source/Engine/RingResonator.cpp
    Source/Engine/RumbleBus.cpp
    Source/Engine/SafetyLimiter.cpp
    Source/Engine/KickVoice.cpp
    Source/Engine/DusterEngine.cpp
    Source/Engine/PreviewRender.cpp
    Source/Primitives/Biquad.cpp
)
add_library(DusterCore STATIC ${DUSTER_CORE_SOURCES})
target_include_directories(DusterCore PUBLIC Source)
# -ffp-contract=off: no fused multiply-add, so a kick renders the same on arm64 and x86_64.
target_compile_options(DusterCore PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
set_target_properties(DusterCore PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---- Plugin ----
if(DUSTER_BUILD_PLUGIN)
    CPMAddPackage(NAME JUCE GITHUB_REPOSITORY juce-framework/JUCE GIT_TAG 8.0.12)

    juce_add_plugin(Duster
        COMPANY_NAME             "LVST"
        PLUGIN_MANUFACTURER_CODE Lvst
        PLUGIN_CODE              Dstr
        FORMATS                  AU VST3
        PRODUCT_NAME             "Duster"
        IS_SYNTH                 TRUE
        NEEDS_MIDI_INPUT         TRUE
        NEEDS_MIDI_OUTPUT        FALSE
        IS_MIDI_EFFECT           FALSE
        COPY_PLUGIN_AFTER_BUILD  ${DUSTER_INSTALL_PLUGIN}
    )

    target_sources(Duster PRIVATE
        Source/Plugin/DusterParams.cpp
        Source/Plugin/DusterPresets.cpp
        Source/Plugin/DusterProcessor.cpp
        Source/Plugin/HostRateResampler.cpp
    )

    target_compile_definitions(Duster PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    )

    target_link_libraries(Duster
        PRIVATE
            DusterCore
            juce::juce_gui_extra
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
    )

    # pluginval (only when the app is installed)
    set(PLUGINVAL_PATH "/Applications/pluginval.app/Contents/MacOS/pluginval" CACHE FILEPATH "pluginval binary")
    if(EXISTS "${PLUGINVAL_PATH}")
        add_test(NAME pluginval_vst3
            COMMAND "${PLUGINVAL_PATH}" --strictness-level 8 --validate-in-process
                    --validate "${CMAKE_BINARY_DIR}/Duster_artefacts/${CMAKE_BUILD_TYPE}/VST3/Duster.vst3")
        set_tests_properties(pluginval_vst3 PROPERTIES LABELS pluginval)
    endif()
    if(DUSTER_INSTALL_PLUGIN) # auval checks the installed component
        add_test(NAME auval_duster COMMAND auval -v aumu Dstr Lvst)
        set_tests_properties(auval_duster PROPERTIES LABELS pluginval)
    endif()
endif()

# ---- Tests (folders mirror Source/) ----
if(DUSTER_BUILD_TESTS)
    CPMAddPackage("gh:catchorg/Catch2@3.7.1")
    add_executable(DusterTests
        tests/Engine/EngineRateTests.cpp
        tests/Engine/KickOscillatorTests.cpp
        tests/Engine/AmpShapeTests.cpp
        tests/Engine/DriveTests.cpp
        tests/Engine/CrusherTests.cpp
        tests/Engine/RingResonatorTests.cpp
        tests/Engine/RumbleBusTests.cpp
        tests/Engine/DusterEngineTests.cpp
        tests/Primitives/FilterTests.cpp
    )
    target_include_directories(DusterTests PRIVATE tests)
    target_link_libraries(DusterTests PRIVATE DusterCore Catch2::Catch2WithMain)
    include(${Catch2_SOURCE_DIR}/extras/Catch.cmake)
    catch_discover_tests(DusterTests)

    # Tests that drive the shipped plugin wrapper (needs JUCE, so only when the plugin is built).
    if(DUSTER_BUILD_PLUGIN)
        add_executable(DusterPluginTests tests/Plugin/PluginSmokeTests.cpp tests/Plugin/PluginRenderTests.cpp)
        target_include_directories(DusterPluginTests PRIVATE tests)
        target_link_libraries(DusterPluginTests PRIVATE
            Duster DusterCore Catch2::Catch2WithMain
            juce::juce_gui_extra juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp
            juce::juce_recommended_config_flags)
        target_compile_definitions(DusterPluginTests PRIVATE JUCE_WEB_BROWSER=1 JUCE_USE_CURL=0)
        catch_discover_tests(DusterPluginTests PROPERTIES LABELS plugin)
    endif()
endif()

# ---- -Werror on our own translation units only (JUCE modules compile inside our targets) ----
if(DUSTER_WERROR)
    target_compile_options(DusterCore PRIVATE -Werror)
    file(GLOB_RECURSE DUSTER_OWN_SOURCES CONFIGURE_DEPENDS
         "${CMAKE_SOURCE_DIR}/Source/*.cpp" "${CMAKE_SOURCE_DIR}/tests/*.cpp" "${CMAKE_SOURCE_DIR}/tools/*.cpp")
    set_source_files_properties(${DUSTER_OWN_SOURCES} PROPERTIES COMPILE_OPTIONS -Werror)
endif()
