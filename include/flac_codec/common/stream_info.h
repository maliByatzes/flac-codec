#pragma once

#include <cstdint>
#include <flac_codec/common/frame_info.h>
#include <vector>

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
  std::vector<uint8_t> m_md5_hash;

  StreamInfo() = default;
  explicit StreamInfo(const std::vector<uint8_t> &bytes);

  void check_values() const;
  void check_frame(FrameInfo &meta) const;
  // void write(bool last, BitOutputStream out);
  // static std::array<uint8_t, 16> get_md5_hash(const std::vector<std::vector<uint8_t>> &sample, int depth);
};

}// namespace flac
