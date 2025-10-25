#pragma once

#include <array>
#include <cstdint>
#include <span>
namespace flac {

class StreamInfo
{
public:
  uint16_t m_min_block_size{};
  uint16_t m_max_block_size{};
  uint32_t m_min_frame_size{};
  uint32_t m_max_frame_size{};
  uint32_t m_sample_rate{};
  uint8_t m_num_channels{};
  uint16_t m_bit_depth{};
  uint64_t m_num_samples{};
  std::array<uint8_t, 16> m_md5_hash{};

  StreamInfo() = default;
  explicit StreamInfo(std::span<const uint8_t> bytes);
};

}// namespace flac
