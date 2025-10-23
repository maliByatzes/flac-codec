#include <flac_codec/common/frame_info.h>
#include <optional>
#include <stdexcept>

namespace flac {

FrameInfo::FrameInfo()
  : m_frame_index(std::nullopt), m_sample_offset(std::nullopt), m_num_channels(std::nullopt),
    m_channel_assignment(std::nullopt), m_block_size(std::nullopt), m_sample_rate(std::nullopt),
    m_bit_depth(std::nullopt), m_frame_size(std::nullopt)
{}

std::optional<FrameInfo> FrameInfo::read_frame(IFlacLowLevelInput &input)
{
  input.reset_crcs();
  auto ttemp = input.read_byte();
  if (!ttemp.has_value()) { return std::nullopt; }
  auto temp = ttemp.value();

  FrameInfo result{};
  result.m_frame_size = std::nullopt;

  auto sync = static_cast<uint16_t>((temp << 6U) | static_cast<uint8_t>(input.read_uint(6)));// NOLINT
  if (sync != 0x3FFE) { throw std::runtime_error("Sync code expected"); }

  if (input.read_uint(1) != 0) { throw std::runtime_error("Reserved bit"); }

  auto blocking_strategy = static_cast<uint8_t>(input.read_uint(1));
  auto block_size_code = static_cast<uint8_t>(input.read_uint(4));
  auto sample_rate_code = static_cast<uint8_t>(input.read_uint(4));
  auto chan_asgn = input.read_uint(4);

  result.m_channel_assignment = static_cast<uint8_t>(chan_asgn);

  if (chan_asgn < 8) {
    result.m_num_channels = static_cast<uint16_t>(chan_asgn + 1);
  } else if (8 <= chan_asgn && chan_asgn <= 10) {
    result.m_num_channels = 2U;
  } else {
    throw std::runtime_error("Reserved channel assignment");
  }

  result.m_bit_depth = decode_bit_depth(static_cast<uint8_t>(input.read_uint(3)));
  if (input.read_uint(1) != 0) { throw std::runtime_error("Reserved bit"); }

  auto position = read_utf8_integer(input);
  if (static_cast<int>(blocking_strategy) != 0) {
    if ((position >> 31) != 0) { throw std::runtime_error("Frame index too large"); }
    result.m_frame_index = position;
    result.m_sample_offset = std::nullopt;
  } else if (static_cast<int>(blocking_strategy) == 1) {
    result.m_sample_offset = position;
    result.m_frame_index = std::nullopt;
  } else {
    throw std::logic_error("Assertion error");
  }

  result.m_block_size = decode_block_size(block_size_code, input);
  result.m_sample_rate = decode_sample_rate(sample_rate_code, input);
  auto computed_crc8 = input.get_crc8();
  if (static_cast<uint8_t>(input.read_uint(8)) != computed_crc8) { throw std::runtime_error("CRC-8 mismatch"); }

  return result;
}

}// namespace flac
