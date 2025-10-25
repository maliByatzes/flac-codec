#include "flac_codec/decode/flac_low_level_input.h"
#include <cstdint>
#include <flac_codec/common/stream_info.h>
#include <span>
#include <stdexcept>

namespace flac {

StreamInfo::StreamInfo(std::span<const uint8_t> bytes)
{
  if (bytes.size() != 34) { throw std::invalid_argument("Invalid data length"); }

  try {
    IFlacLowLevelInput input = ByteArrayFlacInput(bytes);
    m_min_block_size = input.read_uint(16);
    m_max_block_size = input.read_uint(16);
    m_min_frame_size = input.read_uint(24);
    m_max_frame_size = input.read_uint(24);

    if (m_min_block_size < 16) { throw std::runtime_error("Minimum block size less than 16"); }
    if (m_max_block_size > 65535) { throw std::runtime_error("Maximum block size greater than 65535"); }
    if (m_max_block_size < m_min_block_size) {
      throw std::runtime_error("Maximum block size less than minimum block size");
    }
    if (m_min_frame_size != 0 && m_max_frame_size != &&m_max_frame < m_min_frame_size) {}
  } catch (const std::exception &e) {}
}

}// namespace flac
