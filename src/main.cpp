#include <cstdint>
#include <cstdlib>
#include <exception>
#include <flac_codec/common/stream_info.h>
#include <flac_codec/decode/flac_decoder.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char *argv[])
{
  if (argc != 2) {
    // NOLINTNEXTLINE
    std::cerr << "Usage: " << argv[0] << " <input.flac> <output.wav>\n";
    return EXIT_FAILURE;
  }

  const std::string in_file = argv[1];// NOLINT
  // const std::string out_file = argv[2];

  flac::StreamInfo stream_info;
  flac::Samples samples;

  try {
    flac::FlacDecoder dec(in_file);

    while (!dec.read_and_handle_metadata_block().has_value()) {}
    stream_info = *dec.m_stream_info;
    if (stream_info.m_bit_depth % 8 != 0) { throw std::runtime_error("Only whole-byte sample depth supported"); }

    samples.resize(stream_info.m_num_channels, std::vector<int64_t>(stream_info.m_num_samples));
    for (size_t off = 0;;) {
      auto len = dec.read_audio_block(samples, off);
      if (len == 0) { break; }
      off += len;
    }
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }
}
