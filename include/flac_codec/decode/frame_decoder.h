#pragma once

#include <cstdint>
#include <flac_codec/common/frame_info.h>
#include <flac_codec/decode/flac_low_level_input.h>
#include <memory>
#include <optional>
#include <vector>

namespace flac {

class FrameDecoder
{
public:
  std::unique_ptr<IFlacLowLevelInput> m_input;
  uint32_t m_expected_bit_depth{};

  FrameDecoder(std::unique_ptr<IFlacLowLevelInput> &input, uint32_t expect_depth);

  std::optional<FrameInfo> read_frame(std::vector<std::vector<int32_t>> &out_samples, uint64_t out_offset);

private:
  std::vector<int64_t> m_temp0;
  std::vector<int64_t> m_temp1;
  std::optional<uint32_t> m_current_block_size;

  void decode_subframes(uint32_t bit_depth,
    int chan_asgn,
    std::vector<std::vector<int32_t>> &out_samples,
    uint64_t out_offset);
  static int32_t check_bit_depth(uint64_t val, uint32_t depth);
};

}// namespace flac
