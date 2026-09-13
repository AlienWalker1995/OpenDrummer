cmake_minimum_required(VERSION 3.28)
project(OpenDrummer VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime for everything. sfizz forces it for its own targets, and
# mixing it with JUCE's default DLL runtime fails at link time. Static is the
# better choice for a standalone app anyway: no runtime DLLs to install.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)

# --- JUCE ---------------------------------------------------------------------
# Point JUCE_DIR at a local checkout to build offline; left empty, the pinned
# release is downloaded.
set(JUCE_DIR "" CACHE PATH "Optional local JUCE checkout (downloaded when empty)")

if(JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.2
        GIT_SHALLOW    TRUE
        EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(juce)
endif()

# --- sfizz --------------------------------------------------------------------
# The SFZ engine that plays the recorded kits. Only the static library is
# needed: no JACK client, no renderer tool, no plugin wrappers.
set(SFIZZ_JACK OFF CACHE BOOL "" FORCE)
set(SFIZZ_RENDER OFF CACHE BOOL "" FORCE)
set(SFIZZ_SHARED OFF CACHE BOOL "" FORCE)
set(SFIZZ_TESTS OFF CACHE BOOL "" FORCE)
set(SFIZZ_DEMOS OFF CACHE BOOL "" FORCE)
set(SFIZZ_DEVTOOLS OFF CACHE BOOL "" FORCE)
set(SFIZZ_BENCHMARKS OFF CACHE BOOL "" FORCE)

set(SFIZZ_DIR "" CACHE PATH "Optional local sfizz checkout with submodules (downloaded when empty)")

if(SFIZZ_DIR AND EXISTS "${SFIZZ_DIR}/CMakeLists.txt")
    add_subdirectory(${SFIZZ_DIR} ${CMAKE_BINARY_DIR}/sfizz EXCLUDE_FROM_ALL)
else()
    # The release source archive, not a git clone: it already bundles every
    # submodule sfizz needs. Cloning recursively also drags in test-only
    # submodules of its dependencies, and on Windows those fail on path length.
    # Pinned by hash, so a changed download fails loudly instead of building.
    FetchContent_Declare(sfizz
        URL      https://github.com/sfztools/sfizz/releases/download/1.2.3/sfizz-1.2.3.tar.gz
        URL_HASH SHA256=a9339eac7620d7f0f6b44bdfe860680fab73e66efad4b5f15b21198dd9436822
        EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(sfizz)
endif()

# --- the app --------------------------------------------------------------------
juce_add_gui_app(OpenDrummer
    PRODUCT_NAME "OpenDrummer"
    COMPANY_NAME "OpenDrummer"
    # Pixel-art icon, authored at 32x32. The small one is the native grid so
    # Windows never has to shrink pixel art, which would blur it.
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon_pixel_256.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon_pixel_32.png")

target_sources(OpenDrummer PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/DrumEngine.cpp
    Source/MidiMapping.cpp
    Source/SynthKit.cpp
    Source/KitView.cpp
    Source/Theme.cpp
    Source/GrooveLibrary.cpp
    Source/GroovePlayer.cpp
    Source/GroovePanel.cpp
    Source/SampledKits.cpp
    Source/SampledKitPlayer.cpp
    Source/Pixel.cpp
    Source/PixelDeck.cpp)

# --- ASIO -----------------------------------------------------------------------
# The Steinberg ASIO SDK is licence-gated and cannot be redistributed, so it is
# never downloaded or committed. Download it yourself and pass ASIO_SDK_DIR to
# enable ASIO; without it JUCE uses WASAPI, which works with higher latency.
set(ASIO_SDK_DIR "" CACHE PATH "Optional path to the Steinberg ASIO SDK")

if(ASIO_SDK_DIR AND EXISTS "${ASIO_SDK_DIR}/common/iasiodrv.h")
    message(STATUS "ASIO SDK found - enabling ASIO support")
    target_include_directories(OpenDrummer PRIVATE "${ASIO_SDK_DIR}/common")
    target_compile_definitions(OpenDrummer PRIVATE JUCE_ASIO=1)
else()
    message(STATUS "ASIO SDK not set - building with WASAPI only (set ASIO_SDK_DIR for lower latency)")
endif()

target_compile_definitions(OpenDrummer PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:OpenDrummer,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:OpenDrummer,JUCE_VERSION>")

target_link_libraries(OpenDrummer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_dsp
        sfizz::sfizz
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
