cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 23)
endif()

set(CMAKE_CXX_EXTENSIONS OFF)

project(
  flac_codec
  VERSION 0.0.1
  DESCRIPTION "FLAC decoder/encoder"
  # HOMEPAGE_URL https://github.com/maliByatzes/flac-codec
  LANGUAGES CXX
)

include(cmake/PreventInSourceBuilds.cmake)
include(ProjectOptions.cmake)

flac_codec_setup_options()

flac_codec_global_options()
include(Dependencies.cmake)
flac_codec_setup_dependencies()

flac_codec_local_options()

set(GIT_DIR_LOOKUP_POLICY ALLOW_LOOKING_ABOVE_CMAKE_SOURCE_DIR)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(GIT_SHA
    "Unknown"
    CACHE STRING "")
string(
  SUBSTRING "${GIT_SHA}"
            0
            8
            GIT_SHORT_SHA)

target_compile_features(flac_codec_options INTERFACE cxx_std_${CMAKE_CXX_STANDARD})

add_library(flac_codec::flac_codec_options ALIAS flac_codec_options)
add_library(flac_codec::flac_codec_warnings ALIAS flac_codec_warnings)

add_subdirectory(src)

if(NOT PROJECT_IS_TOP_LEVEL)
  return()
endif()

#include(CTest)

#if(BUILD_TESTING)
#  message(AUTHOR_WARNING "Building Tests.")
#  add_subdirectory(test)
#endif()

if(flac_codec_BUILD_FUZZ_TESTS)
  message(AUTHOR_WARNING "Building Fuzz Tests, using fuzzing sanitizer https://www.llvm.org/docs/LibFuzzer.htnl")
  if(NOT flac_codec_ENABLE_SANITIZER_ADDRESS AND NOT flac_codec_ENABLE_SANITIZER_THREAD)
    message(WARNING "You need asan or tsan enabled for meaningful fuzz testing")
  endif()
add_subdirectory(fuzz_test)
endif()

if(MSVC)
  get_all_installable_targets(all_targets)
  message("all_targets=${all_targets}")
  set_target_properties(${all_targets} PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=$(VC_ExecutablePath_x64);%PATH%")
endif()

# set the startup project for the "play" button in MSVC
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT flac_codec)

if(CMAKE_SKIP_INSTALL_RULES)
  return()
endif()

include(cmake/PackageProject.cmake)

flac_codec_package_project(
  TARGETS
  flac_codec
  flac_codec_options
  flac_codec_warnings
)

set(CPACK_PACKAGE_FILE_NAME
    "${CMAKE_PROJECT_NAME}-${CMAKE_PROJECT_VERSION}-${GIT_SHORT_SHA}-${CMAKE_SYSTEM_NAME}-${CMAKE_BUILD_TYPE}-${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
)

include(CPack)
