#pragma once

#include "flac_codec/decode/flac_low_level_input.h"
#include <cstdint>
#include <optional>

namespace flac {

class FrameInfo
{
public:
  FrameInfo();

  static std::optional<FrameInfo> read_frame(IFlacLowLevelInput &input);

  std::optional<uint32_t> m_frame_index;
  std::optional<size_t> m_sample_offset;
  std::optional<uint16_t> m_num_channels;
  std::optional<uint8_t> m_channel_assignment;
  std::optional<uint16_t> m_block_size;
  std::optional<uint32_t> m_sample_rate;
  std::optional<uint16_t> m_bit_depth;
  std::optional<uint32_t> m_frame_size;
};

}// namespace flac
