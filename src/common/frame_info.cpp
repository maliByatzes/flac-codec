#include <bit>
#include <cassert>
#include <cstdint>
#include <flac_codec/common/frame_info.h>
#include <flac_codec/decode/flac_low_level_input.h>
#include <optional>
#include <stdexcept>
#include <vector>

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

  auto position = read_utf8_integer(input).value_or(0);
  if (static_cast<int>(blocking_strategy) != 0) {
    if ((position >> 31U) != 0) { throw std::runtime_error("Frame index too large"); }
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

std::optional<uint64_t> FrameInfo::read_utf8_integer(IFlacLowLevelInput &input)
{
  auto head = static_cast<uint8_t>(input.read_uint(8));
  auto num_leading1s = std::countl_one(head);
  assert(0 <= num_leading1s && num_leading1s <= 8);
  if (num_leading1s == 0) {
    return head;
  } else if (num_leading1s == 1 || num_leading1s == 8) {
    throw std::runtime_error("Invalid UTF-8 coded number");
  } else {
    uint64_t result = head & (0x7FU >> static_cast<uint16_t>(num_leading1s));
    for (int i = 0; i < num_leading1s - 1; ++i) {
      auto temp = static_cast<uint8_t>(input.read_uint(8));
      if ((temp & 0xC0U) != 0x80U) { throw std::runtime_error("Invalid UTF-8 coded number"); }
      result = (result << 6U) | (temp & 0x3FU);
    }

    if ((result >> 36U) != 0) { throw std::logic_error("Assertion error"); }

    return result;
  }
}

uint32_t FrameInfo::decode_block_size(uint8_t code, IFlacLowLevelInput &input)
{
  if ((code >> 4U) != 0) { throw std::invalid_argument("Code argument is invalid"); }

  switch (static_cast<int>(code)) {
  case 0:
    throw std::runtime_error("Reserved block size");
  case 6: {
    auto val = input.read_uint(8) + 1;
    return static_cast<uint32_t>(val);
  }
  case 7: {
    auto val = input.read_uint(16) + 1;
    return static_cast<uint32_t>(val);
  }
  default: {
    auto result = search_second(BLOCK_SIZE_CODES, code).value_or(0);
    if (result < 1 || result > 65536) { throw std::logic_error("Assertion failed."); }
    return result;
  }
  }
}

std::optional<uint32_t> FrameInfo::decode_sample_rate(uint8_t code, IFlacLowLevelInput &input)
{
  if ((code >> 4U) != 0) { throw std::invalid_argument("Code argument is invalid"); }

  switch (static_cast<int>(code)) {
  case 0:
    return std::nullopt;
  case 12: {
    auto val = input.read_uint(8);
    return static_cast<uint32_t>(val);
  }
  case 13: {
    auto val = input.read_uint(16);
    return static_cast<uint32_t>(val);
  }
  case 14: {
    auto val = input.read_uint(16) * 10;
    return static_cast<uint32_t>(val);
  }
  case 15:
    throw std::runtime_error("Invalid sample rate");
  default: {
    auto result = search_second(SAMPLE_RATE_CODES, code);
    if (result < 1 || result > 655350) { throw std::logic_error("Assertion failed"); }
    return result;
  }
  }
}

std::optional<uint16_t> FrameInfo::decode_bit_depth(uint8_t code)
{
  if ((code >> 3U) != 0) {
    throw std::invalid_argument("Code argument is invalid");
  } else if (static_cast<int>(code) == 0) {
    return std::nullopt;
  } else {
    auto result = search_second(BIT_DEPTH_CODES, code);
    if (result == -1) { throw std::runtime_error("Reserved bit depth"); }
    if (result < 1 || result > 32) { throw std::logic_error("Assertion error"); }
    return result;
  }
}

uint8_t FrameInfo::get_block_size_code(uint32_t block_size)
{
  auto tresult = search_first(BLOCK_SIZE_CODES, block_size);
  uint8_t result = 0;
  if (tresult.has_value()) {
    result = tresult.value();
  } else if (1 <= block_size && block_size <= 256) {
    result = 6;
  } else if (1 <= block_size && block_size <= 65536) {
    result = 7;
  } else {
    throw std::invalid_argument("Block size argument is invalid");
  }

  if ((result >> 4U) != 0) { throw std::logic_error("Assertion error"); }

  return result;
}

uint8_t FrameInfo::get_sample_rate_code(uint32_t sample_rate)
{
  if (sample_rate == 0) { throw std::invalid_argument("Sample rate argument is invalid"); }

  auto tresult = search_first(SAMPLE_RATE_CODES, sample_rate);
  uint8_t result = 0;
  if (tresult.has_value()) {
    result = tresult.value();
  } else if (sample_rate < 256) {
    result = 12;
  } else if (sample_rate < 65536) {
    result = 13;
  } else if (sample_rate < 655360 && sample_rate % 10 == 0) {
    result = 14;
  } else {
    result = 0;
  }

  if ((result >> 4U) != 0) { throw std::logic_error("Assertion error"); }

  return result;
}

uint8_t FrameInfo::get_bit_depth_code(uint16_t bit_depth)
{
  if (bit_depth < 1 || bit_depth > 32) { throw std::invalid_argument("Bit depth is not valid"); }

  auto tresult = search_first(BIT_DEPTH_CODES, bit_depth);
  uint8_t result = 0;
  if (!tresult.has_value()) {
    result = 0;
  } else {
    result = tresult.value();
  }

  if ((result >> 3U) != 0) { throw std::logic_error("Assertion error"); }

  return result;
}

std::optional<uint8_t> FrameInfo::search_first(const std::vector<std::vector<uint32_t>> &table, uint32_t key)
{
  for (const auto &pair : table) {
    if (pair[0] == key) { return static_cast<uint8_t>(pair[1]); }
  }
  return std::nullopt;
}

std::optional<uint32_t> search_second(const std::vector<std::vector<uint32_t>> &table, uint32_t key)
{
  for (const auto &pair : table) {
    if (pair[1] == key) { return pair[0]; }
  }
  return std::nullopt;
}

}// namespace flac
