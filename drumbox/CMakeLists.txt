cmake_minimum_required(VERSION 3.22)
project(DrumBox VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The standalone app needs JUCE (fetched) and, on Linux, the X11/ALSA/FreeType
# development packages. The engine and its tests need neither.
option(DRUMBOX_BUILD_APP "Build the JUCE standalone application" ON)

include(CTest)

add_library(drumbox_engine STATIC
    Source/DrumEngine.cpp
    Source/Effects.cpp
    Source/Instruments.cpp
    Source/Presets.cpp
    Source/Sequencer.cpp
    Source/Voices.cpp)
target_include_directories(drumbox_engine PUBLIC Source)
if(MSVC)
    target_compile_options(drumbox_engine PRIVATE /W4)
else()
    target_compile_options(drumbox_engine PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(BUILD_TESTING)
    add_executable(drumbox_tests tests/EngineTests.cpp)
    target_link_libraries(drumbox_tests PRIVATE drumbox_engine)
    if(NOT MSVC)
        target_compile_options(drumbox_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    foreach(id DBX-001 DBX-002 DBX-003 DBX-004 DBX-005 DBX-007 DBX-008 DBX-009 DBX-011 DBX-012 DBX-029 DBX-026 DBX-027 DBX-030 DBX-031 DBX-014 DBX-015 DBX-016)
        add_test(NAME ${id} COMMAND drumbox_tests ${id})
    endforeach()
endif()

if(DRUMBOX_BUILD_APP)
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.6
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)

    juce_add_gui_app(DrumBox
        PRODUCT_NAME "Drum Box"
        COMPANY_NAME "drwil"
        VERSION ${PROJECT_VERSION})

    target_sources(DrumBox PRIVATE
        Source/Main.cpp
        Source/MainComponent.cpp
        Source/FunkLookAndFeel.cpp)

    target_compile_definitions(DrumBox PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:DrumBox,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")

    target_link_libraries(DrumBox PRIVATE
        drumbox_engine
        juce::juce_audio_devices
        juce::juce_gui_basics
        PUBLIC
        juce::juce_recommended_config_flags)
endif()
