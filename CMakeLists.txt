cmake_minimum_required (VERSION 3.22)
file (STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" AURALIS_VERSION LIMIT_COUNT 1)
project (Auralis VERSION ${AURALIS_VERSION} LANGUAGES C CXX)
string (TIMESTAMP AURALIS_RELEASE_DATE "%d.%m.%Y")

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_POSITION_INDEPENDENT_CODE ON)
if (MSVC)
    set (CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option (AURALIS_WITH_WHISPER   "Lyrics recognition with whisper.cpp" ON)
option (AURALIS_WHISPER_VULKAN "Run recognition on the GPU (needs the Vulkan SDK)" OFF)
set (AURALIS_ASIO_SDK "" CACHE PATH "Path to the Steinberg ASIO SDK (enables ASIO in the Windows app)")

# Dependencies: use ext/ if present (offline builds), otherwise download pinned versions.
include (FetchContent)
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ext/JUCE/CMakeLists.txt")
    add_subdirectory (ext/JUCE)
else()
    FetchContent_Declare (juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.8 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable (juce)
endif()

if (AURALIS_WITH_WHISPER)
    set (BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set (WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set (WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set (WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
    set (GGML_NATIVE OFF CACHE BOOL "" FORCE)
    set (GGML_OPENMP OFF CACHE BOOL "" FORCE)
    set (GGML_AVX2 ON CACHE BOOL "" FORCE)
    set (GGML_FMA ON CACHE BOOL "" FORCE)
    set (GGML_F16C ON CACHE BOOL "" FORCE)
    if (AURALIS_WHISPER_VULKAN)
        set (GGML_VULKAN ON CACHE BOOL "" FORCE)
    endif()
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ext/whisper.cpp/CMakeLists.txt")
        add_subdirectory (ext/whisper.cpp EXCLUDE_FROM_ALL)
    else()
        FetchContent_Declare (whisper GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp.git GIT_TAG v1.7.6 GIT_SHALLOW TRUE EXCLUDE_FROM_ALL)
        FetchContent_MakeAvailable (whisper)
    endif()
endif()

juce_add_plugin (Auralis
    COMPANY_NAME            "DJ Maksy"
    BUNDLE_ID               "app.auralis.visualizer"
    PLUGIN_MANUFACTURER_CODE Arls
    PLUGIN_CODE             Avis
    FORMATS                 VST3 Standalone
    PRODUCT_NAME            "Auralis"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    NEEDS_CURL              TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES         Fx Analyzer
    ICON_BIG                "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png")

juce_add_binary_data (AuralisData SOURCES
    Resources/fonts/NotoSans-Bold.ttf
    Resources/fonts/NotoSansArmenian-Bold.ttf
    Resources/fonts/NotoSansGeorgian-Bold.ttf
    Resources/fonts/NotoSansArabic-Bold.ttf)

target_sources (Auralis PRIVATE
    Source/Core/Analyzer.cpp
    Source/Core/FrameProducer.cpp
    Source/Core/Params.cpp
    Source/Core/Presets.cpp
    Source/Core/SceneState.cpp
    Source/Lyrics/Lyrics.cpp
    Source/Lyrics/Recognizer.cpp
    Source/Render/ParticleSims.cpp
    Source/Render/Renderer.cpp
    Source/Render/Ornaments.cpp
    Source/Render/TextLayout.cpp
    Source/Engine/VisualEngine.cpp
    Source/Engine/LiveHub.cpp
    Source/Export/VideoIOCommon.cpp
    Source/Export/VideoIOWindows.cpp
    Source/Export/ExportJob.cpp
    Source/Plugin/PluginProcessor.cpp
    Source/UI/Strings.cpp
    Source/UI/Look.cpp
    Source/UI/GlViews.cpp
    Source/UI/LayerPanel.cpp
    Source/UI/Timeline.cpp
    Source/UI/Editor.cpp
    Source/Standalone/StandaloneApp.cpp)

target_compile_definitions (Auralis PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_WINDOWS_MEDIA_FORMAT=1
    JUCE_STANDALONE_FILTER_WINDOW_USE_KIOSK_MODE=0
    AURALIS_WITH_WHISPER=$<BOOL:${AURALIS_WITH_WHISPER}>
    AURALIS_RELEASE_DATE="${AURALIS_RELEASE_DATE}")

if (WIN32 AND AURALIS_ASIO_SDK)
    target_compile_definitions (Auralis PUBLIC JUCE_ASIO=1)
    target_include_directories (Auralis PUBLIC "${AURALIS_ASIO_SDK}/common")
endif()

target_link_libraries (Auralis
    PRIVATE
        AuralisData
        juce::juce_audio_utils
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

if (AURALIS_WITH_WHISPER)
    target_link_libraries (Auralis PRIVATE whisper)
endif()

if (WIN32)
    target_link_libraries (Auralis PRIVATE mfplat mfreadwrite mfuuid propsys ole32)
endif()
if (MSVC)
    target_compile_options (Auralis PRIVATE /utf-8 /bigobj)
endif()

# ---- unit tests (no JUCE needed)
enable_testing()
add_executable (auralis_core_tests
    tests/core_tests.cpp
    Source/Core/Analyzer.cpp Source/Core/FrameProducer.cpp Source/Core/Params.cpp Source/Core/Presets.cpp
    Source/Lyrics/Lyrics.cpp Source/Render/ParticleSims.cpp)
if (MSVC)
    target_compile_options (auralis_core_tests PRIVATE /utf-8)
endif()
add_test (NAME core COMMAND auralis_core_tests)
