cmake_minimum_required(VERSION 3.25)

# This tells cmake we have goodies in the /cmake folder
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include (PamplejuceVersion)

# Modern concise way to add dependencies to your project
include (CPM)

# Configures universal binaries and decides which version of macOS to support
include(PamplejuceMacOS)

# Couple tweaks that IMO should be JUCE defaults
include(JUCEDefaults)

# Change me!
# This is the internal name of the project and the name of JUCE's shared code target
# Note: This cannot have spaces (it may be 2024, but you can't have it all!)
# Worry not, JUCE's PRODUCT_NAME can have spaces (and is what DAWs display)
set(PROJECT_NAME "StyleStreamerJuce")

# Worry not, JUCE's PRODUCT_NAME can have spaces (and is what DAWs will display)
# You can also just have it be the same thing as PROJECT_NAME
# You may want to append the major version on the end of this (and PROJECT_NAME) ala:
#   set(PROJECT_NAME "MyPlugin_v${MAJOR_VERSION}")
# Doing so enables major versions to show up in IDEs and DAWs as separate plugins
# allowing you to change parameters and behavior without breaking existing user projects
set(PRODUCT_NAME "StyleStreamer")

# Change me! Used for the MacOS bundle name and Installers
set(COMPANY_NAME "Brian Cruz")

# Change me! Used for the MacOS bundle identifier (and signing)
set(BUNDLE_ID "com.rhymeswithlion.stylestreamer")

# Change me! Set the plugin formats you want built
# Valid choices: AAX Unity VST VST3 AU AUv3 Standalone
set(FORMATS Standalone VST3)

# Force JUCE to work with sccache
# instruct MSVC to embed debug info (/Z7) instead of emitting a .pdb
set(CMAKE_POLICY_DEFAULT_CMP0141        NEW      CACHE STRING "" FORCE)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded CACHE STRING "" FORCE)

# For simplicity, the name of the CMake project is also the name of the target
project(${PROJECT_NAME} VERSION ${CURRENT_VERSION})

# Log build environment info (after project() so compiler info is available)
include(PamplejuceLog)

# JUCE is setup as a submodule in the /JUCE folder
# Locally, you must run `git submodule update --init --recursive` once
# and later `git submodule update --remote --merge` to keep it up to date
# On Github Actions, this is done as a part of actions/checkout
add_subdirectory(JUCE)

# Add CLAP format
add_subdirectory(modules/clap-juce-extensions EXCLUDE_FROM_ALL)

# Add any other modules you want modules here, before the juce_add_plugin call
# juce_add_module(modules/my_module)

# This adds the melatonin inspector module
add_subdirectory (modules/melatonin_inspector)

set(MRT_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(MRT_MLX_CPP_DIR "${MRT_REPO_ROOT}/vendor/magenta-realtime-mlx-cpp"
    CACHE PATH "Path to the optimized Magenta RT MLX C++ implementation")

if(NOT DEFINED MLX_ROOT)
    file(GLOB _mrt_root_py_dirs "${MRT_REPO_ROOT}/.venv/lib/python3.*")
    file(GLOB _mrt_vendor_py_dirs "${MRT_MLX_CPP_DIR}/.venv/lib/python3.*")
    if(_mrt_root_py_dirs)
        list(GET _mrt_root_py_dirs 0 _mrt_py_dir)
    elseif(_mrt_vendor_py_dirs)
        list(GET _mrt_vendor_py_dirs 0 _mrt_py_dir)
    else()
        message(FATAL_ERROR
            "MLX_ROOT was not provided and no repo .venv with MLX was found. "
            "Run `uv sync` at the repository root or configure with -DMLX_ROOT=/path/to/site-packages/mlx.")
    endif()
    set(MLX_ROOT "${_mrt_py_dir}/site-packages/mlx"
        CACHE PATH "Directory containing share/cmake/MLX/MLXConfig.cmake")
endif()

list(APPEND CMAKE_PREFIX_PATH "${MLX_ROOT}")
find_package(MLX CONFIG REQUIRED)
set(MRT_RPATH "${MLX_ROOT}/lib" CACHE INTERNAL "")

add_subdirectory(
    "${MRT_MLX_CPP_DIR}/src/magenta_realtime_mlx"
    "${CMAKE_CURRENT_BINARY_DIR}/magenta_realtime_mlx")

target_include_directories(magenta_realtime_mlx
    PUBLIC
    "${MRT_MLX_CPP_DIR}/src")

# The vendor target publishes Homebrew's global include directory because its
# implementation files need SentencePiece and PortAudio headers. Keep that out
# of consumers so tests do not accidentally pick up Homebrew Catch2 headers.
set_target_properties(magenta_realtime_mlx PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${MRT_MLX_CPP_DIR}/src")

# See `docs/CMake API.md` in the JUCE repo for all config options
juce_add_plugin("${PROJECT_NAME}"
    # Icons for the standalone app
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png"

    # Change me!
    COMPANY_NAME "${COMPANY_NAME}"
    BUNDLE_ID "${BUNDLE_ID}"

    # On MacOS, plugin is copied to /Users/yourname/Library/Audio/Plug-Ins/
    COPY_PLUGIN_AFTER_BUILD TRUE

    # Change me!
    # A four-character plugin id
    # First character MUST be uppercase for AU formats
    PLUGIN_MANUFACTURER_CODE MRTJ

    # Change me!
    # A unique four-character plugin id
    # Note: this must have at least one upper-case character
    PLUGIN_CODE Mr01
    FORMATS "${FORMATS}"

    # The name of your final executable
    # This is how it's listed in the DAW
    # This can be different from PROJECT_NAME and can have spaces!
    # You might want to use v${MAJOR_VERSION} here once you go to v2...
    PRODUCT_NAME "${PRODUCT_NAME}")

# This lets us use our code in both the JUCE targets and our Test target
# Without running into ODR violations
add_library(SharedCode INTERFACE)

clap_juce_extensions_plugin(TARGET "${PROJECT_NAME}"
    CLAP_ID "${BUNDLE_ID}"
    CLAP_FEATURES audio-effect)

# Enable fast math, C++20 and a few other target defaults
include(SharedCodeDefaults)


# Manually list all .h and .cpp files for the plugin
# If you are like me, you'll use globs for your sanity.
# Just ensure you employ CONFIGURE_DEPENDS so the build system picks up changes
# If you want to appease the CMake gods and avoid globs, manually add files like so:
# set(SourceFiles Source/PluginEditor.h Source/PluginProcessor.h Source/PluginEditor.cpp Source/PluginProcessor.cpp)
file(GLOB_RECURSE SourceFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h")
target_sources(SharedCode INTERFACE ${SourceFiles})
target_include_directories(SharedCode
    INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/source")

# Adds a BinaryData target for embedding assets into the binary
include(Assets)

# MacOS only: Cleans up folder and target organization on Xcode.
include(XcodePrettify)

# This is where you can set preprocessor definitions for JUCE and your plugin
target_compile_definitions(SharedCode
    INTERFACE

    # JUCE_WEB_BROWSER and JUCE_USE_CURL off by default
    JUCE_WEB_BROWSER=0  # If you set this to 1, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
    JUCE_USE_CURL=0     # If you set this to 1, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
    JUCE_VST3_CAN_REPLACE_VST2=0

    # Uncomment if you are paying for a an Indie/Pro license or releasing under GPLv3
    # JUCE_DISPLAY_SPLASH_SCREEN=0

    # lets the app known if we're Debug or Release
    CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    VERSION="${CURRENT_VERSION}"

    # JucePlugin_Name is for some reason doesn't use the nicer PRODUCT_NAME
    PRODUCT_NAME_WITHOUT_VERSION="StyleStreamer"
)

# Link to any other modules you added (with juce_add_module) here!
# Usually JUCE modules must have PRIVATE visibility
# See https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md#juce_add_module
# However, with Pamplejuce, you'll link modules to SharedCode with INTERFACE visibility
# This allows the JUCE plugin targets and the Tests target to link against it
target_link_libraries(SharedCode
    INTERFACE
    Assets
    magenta_realtime_mlx
    melatonin_inspector
    juce_audio_utils
    juce_audio_processors
    juce_dsp
    juce_gui_basics
    juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# Link the JUCE plugin targets our SharedCode target
target_link_libraries("${PROJECT_NAME}" PRIVATE SharedCode)

if(UNIX)
    # Resolve MLX from the configured wheel in development builds. Distribution
    # targets may still rewrite/bundle native libraries for portable artefacts.
    foreach(_mrt_runtime_target
            "${PROJECT_NAME}_Standalone"
            "${PROJECT_NAME}_VST3")
        if(TARGET "${_mrt_runtime_target}")
            set_target_properties("${_mrt_runtime_target}" PROPERTIES
                INSTALL_RPATH "${MRT_RPATH}"
                BUILD_WITH_INSTALL_RPATH ON)
        endif()
    endforeach()
endif()

# IPP support, comment out to disable
include(PamplejuceIPP)

# Everything related to the tests target
include(Tests)

# A separate target for Benchmarks (keeps the Tests target fast)
include(Benchmarks)

# Output some config for CI (like our PRODUCT_NAME)
include(GitHubENV)
