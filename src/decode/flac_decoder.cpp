#include "flac_codec/common/frame_info.h"
#include <flac_codec/common/seek_table.h>
#include <flac_codec/common/stream_info.h>
#include <flac_codec/decode/flac_decoder.h>
#include <flac_codec/decode/seekable_file_flac_input.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace flac {

FlacDecoder::FlacDecoder(const std::string &file_name)
{
  m_input = std::make_unique<SeekableFileFlacInput>(file_name);

  if (static_cast<uint32_t>(m_input->read_uint(32)) != 0x664C6143) { throw std::logic_error("Invalid magic string"); }

  m_metadata_end_pos = std::nullopt;
}

std::optional<std::pair<uint8_t, std::vector<uint8_t>>> FlacDecoder::read_and_handle_metadata_block()
{
  if (m_metadata_end_pos == std::nullopt) { return std::nullopt; }

  bool last = static_cast<int>(m_input->read_uint(1)) != 0;
  auto type = static_cast<uint8_t>(m_input->read_uint(7));
  auto length = static_cast<uint32_t>(m_input->read_uint(24));
  std::vector<uint8_t> data(length);
  m_input->read_fully(data);

  if (static_cast<int>(type) == 0) {
    if (m_stream_info != nullptr) { throw std::logic_error("Duplicate stream info metadata block"); }
    m_stream_info = std::make_unique<StreamInfo>(data);
  } else {
    if (m_stream_info == nullptr) { throw std::logic_error("Expected stream info metadata block"); }
    if (static_cast<int>(type) == 3) {
      if (m_seek_table != nullptr) { throw std::logic_error("Duplicate seek table metadata block"); }
      m_seek_table = std::make_unique<SeekTable>(data);
    }
  }

  if (last) {
    m_metadata_end_pos = m_input->get_position();
    // m_frame_dec = std::make_unique<FrameDecoder>(m_input, m_stream_info->bit_depth);
  }

  return std::make_pair(type, data);
}

uint32_t FlacDecoder::read_audio_block([[maybe_unused]] std::vector<std::vector<uint32_t>> &samples,
  [[maybe_unused]] int off)
{
  /*
  if (m_frame_dec == nullptr) { throw std::logic_error("Metadata blocks not fully consumed yet"); }
  auto frame = m_frame_dec->read_frame(samples, off);
  if (frame == nullptr) {
    return 0;
  } else {
    return frame.block_size;
  }*/
  return 0;
}
}// namespace flac
