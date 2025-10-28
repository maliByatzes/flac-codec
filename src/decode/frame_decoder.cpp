#include <cassert>
#include <cstddef>
#include <flac_codec/common/frame_info.h>
#include <flac_codec/decode/frame_decoder.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace flac {

FrameDecoder::FrameDecoder(std::unique_ptr<IFlacLowLevelInput> &input, uint32_t expect_depth)
  : m_input(std::move(input)), m_expected_bit_depth(expect_depth), m_temp0(65536), m_temp1(65536),
    m_current_block_size(std::nullopt)
{}

std::optional<FrameInfo> FrameDecoder::read_frame(std::vector<std::vector<int32_t>> &out_samples, uint64_t out_offset)
{
  if (!m_current_block_size.has_value()) { throw std::logic_error("Concurrent call"); }

  auto start_byte = m_input->get_position();
  auto tmeta = FrameInfo::read_frame(*m_input);
  if (!tmeta.has_value()) { return std::nullopt; }
  auto meta = tmeta.value();
  if (meta.m_bit_depth.has_value() && meta.m_bit_depth.value() != m_expected_bit_depth) {
    throw std::logic_error("Bit depth mismatch");
  }

  m_current_block_size = meta.m_block_size;
  if (out_offset > out_samples[0].size()) { throw std::logic_error("Index is out of bounds"); }
  if (out_samples.size() < meta.m_num_channels.value_or(0)) {
    throw std::invalid_argument("Output array too small for number of channels");
  }
  if (out_offset > out_samples[0].size() - m_current_block_size.value_or(0)) {
    throw std::logic_error("Index is out of bounds");
  }

  decode_subframes(m_expected_bit_depth, meta.m_channel_assignment.value_or(0), out_samples, out_offset);

  if (m_input->read_uint((8 - m_input->get_position().value_or(0)) % 8) != 0) {
    throw std::logic_error("Invalid padding bits");
  }
  auto computed_crc16 = m_input->get_crc16();
  if (m_input->read_uint(16) != computed_crc16) { throw std::logic_error("CRC-16 mismatch"); }

  auto frame_size = m_input->get_position().value_or(0) - start_byte.value_or(0);
  if (frame_size < 10) { throw std::logic_error("Assertion error"); }
  if (static_cast<uint32_t>(frame_size) != frame_size) { throw std::logic_error("Frame size too large"); }

  meta.m_frame_size = static_cast<uint32_t>(frame_size);
  m_current_block_size = std::nullopt;

  return meta;
}

void FrameDecoder::decode_subframes(uint32_t bit_depth,
  int chan_asgn,
  std::vector<std::vector<int32_t>> &out_samples,
  uint64_t out_offset)
{
  if (bit_depth < 1 || bit_depth > 32) { throw std::invalid_argument("Bit depth is invalid"); }
  if ((static_cast<uint8_t>(chan_asgn) >> 4U) != 0) { throw std::invalid_argument("Channel assignment is invalid"); }

  if (0 <= chan_asgn && chan_asgn <= 7) {
    int num_channels = chan_asgn + 1;
    for (size_t ch = 0; ch < size_t(num_channels); ++ch) {
      decode_subframe(bit_depth, m_temp0);
      auto out_chan = out_samples[ch];
      for (size_t i = 0; i < m_current_block_size.value_or(0); ++i) {
        out_chan[out_offset + i] = check_bit_depth(m_temp0[0], bit_depth);
      }
    }
  } else if (8 <= chan_asgn && chan_asgn <= 10) {
    decode_subframe(bit_depth + (chan_asgn == 9 ? 1 : 0), m_temp0);
    decode_subframe(bit_depth + (chan_asgn == 9 ? 0 : 1), m_temp1);

    if (chan_asgn == 8) {
      for (size_t i = 0; i < m_current_block_size.value_or(0); ++i) { m_temp1[i] = m_temp0[i] - m_temp1[i]; }
    } else if (chan_asgn == 9) {
      for (size_t i = 0; i < m_current_block_size.value_or(0); ++i) { m_temp0[i] += m_temp1[i]; }
    } else if (chan_asgn == 10) {
      for (size_t i = 0; i < m_current_block_size.value_or(0); ++i) {
        auto side = static_cast<uint64_t>(m_temp1[i]);
        auto right = static_cast<uint64_t>(m_temp0[i]) - (side >> 1U);
        m_temp1[i] = static_cast<long>(right);
        m_temp0[i] = static_cast<long>(right + side);
      }
    } else {
      throw std::logic_error("Assertion error");
    }
    auto out_left = out_samples[0];
    auto out_right = out_samples[1];
    for (size_t i = 0; i < m_current_block_size.value_or(0); ++i) {
      out_left[out_offset + i] = check_bit_depth(m_temp0[i], bit_depth);
      out_right[out_offset + i] = check_bit_depth(m_temp1[i], bit_depth);
    }
  } else {
    throw std::logic_error("Reserved channel assignment");
  }
}

int32_t FrameDecoder::check_bit_depth(uint64_t val, uint32_t depth)
{
  assert(1 <= depth && depth <= 32);

  if (val >> (depth - 1U) == val >> depth) {
    return static_cast<int32_t>(val);
  } else {
    const std::string msg(std::to_string(val) + " is not a signed " + std::to_string(depth) + "-bit value");
    throw std::invalid_argument(msg);
  }
}
}// namespace flac
