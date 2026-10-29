cmake_minimum_required(VERSION 3.22)
project(GlacierRiser VERSION 2.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: prefer a local checkout, otherwise fetch 8.0.4 from GitHub.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../EchoBox/JUCE/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../EchoBox/JUCE" "${CMAKE_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# WebView2 SDK (NuGet package Microsoft.Web.WebView2), downloaded into third_party/webview2 on
# first configure so the repository does not carry Microsoft's binaries.
set(WEBVIEW2_VERSION 1.0.1901.177)
set(JUCE_WEBVIEW2_PACKAGE_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/third_party/webview2")
set(WEBVIEW2_DIR "${JUCE_WEBVIEW2_PACKAGE_LOCATION}/Microsoft.Web.WebView2.${WEBVIEW2_VERSION}")

if(WIN32 AND NOT EXISTS "${WEBVIEW2_DIR}/build/native/include/WebView2.h")
    message(STATUS "Downloading the WebView2 SDK ${WEBVIEW2_VERSION} from nuget.org...")
    set(WEBVIEW2_PACKAGE "${CMAKE_BINARY_DIR}/Microsoft.Web.WebView2.${WEBVIEW2_VERSION}.nupkg")
    file(DOWNLOAD "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${WEBVIEW2_VERSION}"
         "${WEBVIEW2_PACKAGE}" STATUS webview2_status TLS_VERIFY ON)
    list(GET webview2_status 0 webview2_code)
    if(NOT webview2_code EQUAL 0)
        message(FATAL_ERROR "Could not download the WebView2 SDK: ${webview2_status}")
    endif()
    file(ARCHIVE_EXTRACT INPUT "${WEBVIEW2_PACKAGE}" DESTINATION "${WEBVIEW2_DIR}")
endif()

# ------------------------------------------------------------------------------
# Web UI: ui/ is a Vite + React app that builds into one self-contained index.html.
find_program(NPM_EXECUTABLE NAMES npm.cmd npm REQUIRED)
set(UI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ui")
set(UI_BUNDLE "${UI_DIR}/dist/index.html")

if(NOT EXISTS "${UI_DIR}/node_modules")
    message(STATUS "Installing UI dependencies (npm install)...")
    execute_process(COMMAND "${NPM_EXECUTABLE}" install
                    WORKING_DIRECTORY "${UI_DIR}"
                    RESULT_VARIABLE npm_install_result)
    if(NOT npm_install_result EQUAL 0)
        message(FATAL_ERROR "npm install failed in ${UI_DIR}")
    endif()
endif()

file(GLOB_RECURSE UI_SOURCES CONFIGURE_DEPENDS
    "${UI_DIR}/src/*"
    "${UI_DIR}/index.html"
    "${UI_DIR}/vite.config.ts"
    "${UI_DIR}/package.json")

add_custom_command(
    OUTPUT "${UI_BUNDLE}"
    COMMAND "${NPM_EXECUTABLE}" run build
    WORKING_DIRECTORY "${UI_DIR}"
    DEPENDS ${UI_SOURCES}
    COMMENT "Building the Glacier Riser web UI"
    VERBATIM)

add_custom_target(GlacierRiserUIBundle DEPENDS "${UI_BUNDLE}")

juce_add_binary_data(GlacierRiserUIData
    HEADER_NAME GlacierRiserUIData.h
    NAMESPACE GlacierRiserUIData
    SOURCES "${UI_BUNDLE}")

add_dependencies(GlacierRiserUIData GlacierRiserUIBundle)

# ------------------------------------------------------------------------------
juce_add_plugin(GlacierRiser
    COMPANY_NAME "Mykhailo Nosenko"
    PLUGIN_MANUFACTURER_CODE Mnos
    PLUGIN_CODE Rvrb
    FORMATS VST3 Standalone
    PRODUCT_NAME "Glacier Riser"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.svg"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    NEEDS_WEBVIEW2 TRUE
    VST3_CATEGORIES Fx Reverb
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(GlacierRiser PRIVATE
    Source/ReverseReverbRenderer.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(GlacierRiser PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(GlacierRiser
    PRIVATE
        GlacierRiserUIData
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ------------------------------------------------------------------------------
# Optional end-to-end check: hosts the built VST3 like a DAW (capture, then in-sync playback).
#   cmake --build build --config Release --target GlacierRiserHostCheck
#   build/GlacierRiserHostCheck_artefacts/Release/GlacierRiserHostCheck.exe build/GlacierRiser_artefacts/Release/"VST3/Glacier Riser.vst3"
juce_add_console_app(GlacierRiserHostCheck PRODUCT_NAME "GlacierRiserHostCheck")

target_sources(GlacierRiserHostCheck PRIVATE
    tests/HostCheck.cpp
    Source/ReverseReverbRenderer.cpp)

target_compile_definitions(GlacierRiserHostCheck PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(GlacierRiserHostCheck PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_gui_basics
    juce::juce_recommended_config_flags)

set_target_properties(GlacierRiserHostCheck PROPERTIES EXCLUDE_FROM_ALL TRUE)
