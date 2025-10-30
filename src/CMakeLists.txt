add_library(flac_codec_lib
    decode/flac_low_level_input.cpp
    decode/byte_flac_input.cpp
    decode/seekable_file_flac_input.cpp
    decode/flac_decoder.cpp
    decode/frame_decoder.cpp

    common/frame_info.cpp
    common/seek_table.cpp
    common/stream_info.cpp
)

target_link_libraries(flac_codec_lib
  PRIVATE flac_codec::flac_codec_options
          flac_codec::flac_codec_warnings
)

#target_link_system_libraries(
#  flac_codec_lib
#  PRIVATE
#)

target_include_directories(flac_codec_lib 
  ${WARNING_GUARD} PUBLIC 
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
)

target_compile_features(flac_codec_lib PUBLIC cxx_std_23)

add_library(flac_codec::flac_codec_lib ALIAS flac_codec_lib)

add_executable(flac_codec
  main.cpp
)

target_link_libraries(flac_codec
  PRIVATE flac_codec::flac_codec_lib
)
