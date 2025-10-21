include(cmake/SystemLink.cmake)
include(cmake/LibFuzzer.cmake)
include(CMakeDependentOption)
include(CheckCXXCompilerFlag)


include(CheckCXXSourceCompiles)


macro(flac_codec_supports_sanitizers)
  if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND NOT WIN32)

    message(STATUS "Sanity checking UndefinedBehaviorSanitizer, it should be supported on this platform")
    set(TEST_PROGRAM "int main() { return 0; }")

    # Check if UndefinedBehaviorSanitizer works at link time
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=undefined")
    set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=undefined")
    check_cxx_source_compiles("${TEST_PROGRAM}" HAS_UBSAN_LINK_SUPPORT)

    if(HAS_UBSAN_LINK_SUPPORT)
      message(STATUS "UndefinedBehaviorSanitizer is supported at both compile and link time.")
      set(SUPPORTS_UBSAN ON)
    else()
      message(WARNING "UndefinedBehaviorSanitizer is NOT supported at link time.")
      set(SUPPORTS_UBSAN OFF)
    endif()
  else()
    set(SUPPORTS_UBSAN OFF)
  endif()

  if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND WIN32)
    set(SUPPORTS_ASAN OFF)
  else()
    if (NOT WIN32)
      message(STATUS "Sanity checking AddressSanitizer, it should be supported on this platform")
      set(TEST_PROGRAM "int main() { return 0; }")

      # Check if AddressSanitizer works at link time
      set(CMAKE_REQUIRED_FLAGS "-fsanitize=address")
      set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=address")
      check_cxx_source_compiles("${TEST_PROGRAM}" HAS_ASAN_LINK_SUPPORT)

      if(HAS_ASAN_LINK_SUPPORT)
        message(STATUS "AddressSanitizer is supported at both compile and link time.")
        set(SUPPORTS_ASAN ON)
      else()
        message(WARNING "AddressSanitizer is NOT supported at link time.")
        set(SUPPORTS_ASAN OFF)
      endif()
    else()
      set(SUPPORTS_ASAN ON)
    endif()
  endif()
endmacro()

macro(flac_codec_setup_options)
  option(flac_codec_ENABLE_HARDENING "Enable hardening" ON)
  option(flac_codec_ENABLE_COVERAGE "Enable coverage reporting" OFF)
  cmake_dependent_option(
    flac_codec_ENABLE_GLOBAL_HARDENING
    "Attempt to push hardening options to built dependencies"
    ON
    flac_codec_ENABLE_HARDENING
    OFF)

  flac_codec_supports_sanitizers()

  if(NOT PROJECT_IS_TOP_LEVEL OR flac_codec_PACKAGING_MAINTAINER_MODE)
    option(flac_codec_ENABLE_IPO "Enable IPO/LTO" OFF)
    option(flac_codec_WARNINGS_AS_ERRORS "Treat Warnings As Errors" OFF)
    option(flac_codec_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(flac_codec_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" OFF)
    option(flac_codec_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(flac_codec_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" OFF)
    option(flac_codec_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(flac_codec_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(flac_codec_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(flac_codec_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
    option(flac_codec_ENABLE_CPPCHECK "Enable cpp-check analysis" OFF)
    option(flac_codec_ENABLE_PCH "Enable precompiled headers" OFF)
    option(flac_codec_ENABLE_CACHE "Enable ccache" OFF)
  else()
    option(flac_codec_ENABLE_IPO "Enable IPO/LTO" ON)
    option(flac_codec_WARNINGS_AS_ERRORS "Treat Warnings As Errors" ON)
    option(flac_codec_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(flac_codec_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" ${SUPPORTS_ASAN})
    option(flac_codec_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(flac_codec_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" ${SUPPORTS_UBSAN})
    option(flac_codec_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(flac_codec_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(flac_codec_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(flac_codec_ENABLE_CLANG_TIDY "Enable clang-tidy" ON)
    option(flac_codec_ENABLE_CPPCHECK "Enable cpp-check analysis" ON)
    option(flac_codec_ENABLE_PCH "Enable precompiled headers" OFF)
    option(flac_codec_ENABLE_CACHE "Enable ccache" ON)
  endif()

  if(NOT PROJECT_IS_TOP_LEVEL)
    mark_as_advanced(
      flac_codec_ENABLE_IPO
      flac_codec_WARNINGS_AS_ERRORS
      flac_codec_ENABLE_USER_LINKER
      flac_codec_ENABLE_SANITIZER_ADDRESS
      flac_codec_ENABLE_SANITIZER_LEAK
      flac_codec_ENABLE_SANITIZER_UNDEFINED
      flac_codec_ENABLE_SANITIZER_THREAD
      flac_codec_ENABLE_SANITIZER_MEMORY
      flac_codec_ENABLE_UNITY_BUILD
      flac_codec_ENABLE_CLANG_TIDY
      flac_codec_ENABLE_CPPCHECK
      flac_codec_ENABLE_COVERAGE
      flac_codec_ENABLE_PCH
      flac_codec_ENABLE_CACHE)
  endif()

  flac_codec_check_libfuzzer_support(LIBFUZZER_SUPPORTED)
  if(LIBFUZZER_SUPPORTED AND (flac_codec_ENABLE_SANITIZER_ADDRESS OR flac_codec_ENABLE_SANITIZER_THREAD OR flac_codec_ENABLE_SANITIZER_UNDEFINED))
    set(DEFAULT_FUZZER ON)
  else()
    set(DEFAULT_FUZZER OFF)
  endif()

  option(flac_codec_BUILD_FUZZ_TESTS "Enable fuzz testing executable" ${DEFAULT_FUZZER})

endmacro()

macro(flac_codec_global_options)
  if(flac_codec_ENABLE_IPO)
    include(cmake/InterproceduralOptimization.cmake)
    flac_codec_enable_ipo()
  endif()

  flac_codec_supports_sanitizers()

  if(flac_codec_ENABLE_HARDENING AND flac_codec_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR flac_codec_ENABLE_SANITIZER_UNDEFINED
       OR flac_codec_ENABLE_SANITIZER_ADDRESS
       OR flac_codec_ENABLE_SANITIZER_THREAD
       OR flac_codec_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    message("${flac_codec_ENABLE_HARDENING} ${ENABLE_UBSAN_MINIMAL_RUNTIME} ${flac_codec_ENABLE_SANITIZER_UNDEFINED}")
    flac_codec_enable_hardening(flac_codec_options ON ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()
endmacro()

macro(flac_codec_local_options)
  if(PROJECT_IS_TOP_LEVEL)
    include(cmake/StandardProjectSettings.cmake)
  endif()

  add_library(flac_codec_warnings INTERFACE)
  add_library(flac_codec_options INTERFACE)

  include(cmake/CompilerWarnings.cmake)
  flac_codec_set_project_warnings(
    flac_codec_warnings
    ${flac_codec_WARNINGS_AS_ERRORS}
    ""
    ""
    ""
    "")

  if(flac_codec_ENABLE_USER_LINKER)
    include(cmake/Linker.cmake)
    flac_codec_configure_linker(flac_codec_options)
  endif()

  include(cmake/Sanitizers.cmake)
  flac_codec_enable_sanitizers(
    flac_codec_options
    ${flac_codec_ENABLE_SANITIZER_ADDRESS}
    ${flac_codec_ENABLE_SANITIZER_LEAK}
    ${flac_codec_ENABLE_SANITIZER_UNDEFINED}
    ${flac_codec_ENABLE_SANITIZER_THREAD}
    ${flac_codec_ENABLE_SANITIZER_MEMORY})

  set_target_properties(flac_codec_options PROPERTIES UNITY_BUILD ${flac_codec_ENABLE_UNITY_BUILD})

  if(flac_codec_ENABLE_PCH)
    target_precompile_headers(
      flac_codec_options
      INTERFACE
      <vector>
      <string>
      <utility>)
  endif()

  if(flac_codec_ENABLE_CACHE)
    include(cmake/Cache.cmake)
    flac_codec_enable_cache()
  endif()

  include(cmake/StaticAnalyzers.cmake)
  if(flac_codec_ENABLE_CLANG_TIDY)
    flac_codec_enable_clang_tidy(flac_codec_options ${flac_codec_WARNINGS_AS_ERRORS})
  endif()

  if(flac_codec_ENABLE_CPPCHECK)
    flac_codec_enable_cppcheck(${flac_codec_WARNINGS_AS_ERRORS} "" # override cppcheck options
    )
  endif()

  if(flac_codec_ENABLE_COVERAGE)
    include(cmake/Tests.cmake)
    flac_codec_enable_coverage(flac_codec_options)
  endif()

  if(flac_codec_WARNINGS_AS_ERRORS)
    check_cxx_compiler_flag("-Wl,--fatal-warnings" LINKER_FATAL_WARNINGS)
    if(LINKER_FATAL_WARNINGS)
      # This is not working consistently, so disabling for now
      # target_link_options(flac_codec_options INTERFACE -Wl,--fatal-warnings)
    endif()
  endif()

  if(flac_codec_ENABLE_HARDENING AND NOT flac_codec_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR flac_codec_ENABLE_SANITIZER_UNDEFINED
       OR flac_codec_ENABLE_SANITIZER_ADDRESS
       OR flac_codec_ENABLE_SANITIZER_THREAD
       OR flac_codec_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    flac_codec_enable_hardening(flac_codec_options OFF ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()

endmacro()
