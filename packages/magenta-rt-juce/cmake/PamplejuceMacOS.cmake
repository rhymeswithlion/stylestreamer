
# This must be set before the project() call
# see: https://cmake.org/cmake/help/latest/variable/CMAKE_OSX_DEPLOYMENT_TARGET.html
# FORCE must be set, see https://stackoverflow.com/a/44340246
#
# Must match the bundled libmlx.dylib, which is built for macOS 26.0.
# Setting a lower target produces dyld load-time errors because the dylib
# refuses to link against an older-targeted binary.
set(CMAKE_OSX_DEPLOYMENT_TARGET "26.0" CACHE STRING "Minimum macOS for StyleStreamer (26.0, matches libmlx)" FORCE)

# Building universal binaries on macOS increases build time
# This is set on CI but not during local dev
if ((DEFINED ENV{CI}) AND (CMAKE_BUILD_TYPE STREQUAL "Release") AND NOT (CMAKE_SYSTEM_NAME STREQUAL "iOS"))
    message("Building for Apple Silicon and x86_64")
    set(CMAKE_OSX_ARCHITECTURES arm64 x86_64)
endif ()

# By default we don't want Xcode schemes to be made for modules, etc
set(CMAKE_XCODE_GENERATE_SCHEME OFF)
