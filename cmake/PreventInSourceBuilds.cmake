function(flac_codec_assure_out_of_source_builds)
  get_filename_component(srcdir "${CMAKE_SOURCE_DIR}" REALPATH)
  get_filename_component(bindir "${CMAKE_BINARY_DIR}" REALPATH)

  if("${srcdir}" STREQUAL "${bindir}")
    message("Warning: in-source builds are disabled")
    message("Create a separate build directory and run cmake from there")
    message(FATAL_ERROR "Quitting")
  endif()
endfunction()

flac_codec_assure_out_of_source_builds()
