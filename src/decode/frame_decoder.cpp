#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <flac_codec/common/frame_info.h>
#include <flac_codec/decode/data_format_exception.h>
#include <flac_codec/decode/flac_low_level_input.h>
#include <flac_codec/decode/frame_decoder.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace flac {

FrameDecoder::FrameDecoder(std::unique_ptr<IFlacLowLevelInput> &input, uint32_t expect_depth)
  : m_input(std::move(input)), m_expected_bit_depth(expect_depth), m_temp0(65536), m_temp1(65536),
    m_current_block_size(std::nullopt)
{}

std::optional<FrameInfo> FrameDecoder::read_frame(std::vector<std::vector<int64_t>> &out_samples, size_t out_offset)
{
  if (!m_current_block_size.has_value()) { throw std::runtime_error("Concurrent call"); }

  auto start_byte = m_input->get_position();
  auto tmeta = FrameInfo::read_frame(*m_input);
  if (!tmeta.has_value()) { return std::nullopt; }
  auto meta = tmeta.value();
  if (!meta.m_bit_depth.has_value() && meta.m_bit_depth.value_or(0) != m_expected_bit_depth) {
    throw DataFormatException("Bit depth mismatch");
  }

  m_current_block_size = meta.m_block_size;
  if (out_offset > out_samples[0].size()) { throw std::runtime_error("Index is out of bounds"); }
  if (out_samples.size() < meta.m_num_channels.value_or(0)) {
    throw std::invalid_argument("Output array too small for number of channels");
  }
  if (out_offset > out_samples[0].size() - m_current_block_size.value_or(0)) {
    throw std::runtime_error("Index is out of bounds");
  }

  decode_subframes(m_expected_bit_depth, meta.m_channel_assignment.value_or(0), out_samples, out_offset);

  if (m_input->read_uint((8 - m_input->get_position()) % 8) != 0) { throw DataFormatException("Invalid padding bits"); }
  auto computed_crc16 = m_input->get_crc16();
  if (m_input->read_uint(16) != computed_crc16) { throw DataFormatException("CRC-16 mismatch"); }

  auto frame_size = m_input->get_position() - start_byte;
  if (frame_size < 10) { throw std::runtime_error("Assertion error"); }
  if (static_cast<uint32_t>(frame_size) != frame_size) { throw DataFormatException("Frame size too large"); }

  meta.m_frame_size = static_cast<uint32_t>(frame_size);
  m_current_block_size = std::nullopt;

  return meta;
}

void FrameDecoder::decode_subframes(uint32_t bit_depth,
  int chan_asgn,
  std::vector<std::vector<int64_t>> &out_samples,
  size_t out_offset)
{
  if (bit_depth < 1 || bit_depth > 32) { throw std::invalid_argument("Bit depth is invalid"); }
  if ((static_cast<uint8_t>(chan_asgn) >> 4U) != 0) { throw std::invalid_argument("Channel assignment is invalid"); }

  if (0 <= chan_asgn && chan_asgn <= 7) {
    const int num_channels = chan_asgn + 1;
    for (size_t ch = 0; std::cmp_less(ch, num_channels); ++ch) {
      decode_subframe(bit_depth, m_temp0);
      auto out_chan = out_samples[ch];
      for (size_t i = 0; i < m_current_block_size.value_or(0); ++i) {
        out_chan[out_offset + i] = check_bit_depth(m_temp0[i], bit_depth);
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
      throw std::runtime_error("Assertion error");
    }

    auto out_left = out_samples[0];
    auto out_right = out_samples[1];
    for (size_t i = 0; i < m_current_block_size.value_or(0); ++i) {
      out_left[out_offset + i] = check_bit_depth(m_temp0[i], bit_depth);
      out_right[out_offset + i] = check_bit_depth(m_temp1[i], bit_depth);
    }
  } else {
    throw DataFormatException("Reserved channel assignment");
  }
}

int32_t FrameDecoder::check_bit_depth(int64_t val, uint32_t depth)
{
  assert(1 <= depth && depth <= 32);

  auto value = static_cast<uint64_t>(val);

  if (value >> (depth - 1U) == value >> depth) {
    return static_cast<int32_t>(value);
  } else {
    const std::string msg(std::to_string(value) + " is not a signed " + std::to_string(depth) + "-bit value");
    throw std::invalid_argument(msg);
  }
}

void FrameDecoder::decode_subframe(uint32_t bit_depth, std::vector<int64_t> &result)
{
  if (bit_depth < 1 || bit_depth > 33) { throw std::invalid_argument("bit_depth is invalid"); }
  if (result.size() < m_current_block_size.value_or(0)) { throw std::invalid_argument("result is invalid"); }

  if (m_input->read_uint(1) != 0) { throw DataFormatException("Invalid padding bit"); }

  auto type = m_input->read_uint(6);
  auto shift = m_input->read_uint(1);

  if (shift == 1) {
    while (m_input->read_uint(1) == 0) {
      if (std::cmp_greater_equal(shift, bit_depth)) {
        throw DataFormatException("Waste-bits-per-samples exceeds bit depth");
      }
      shift++;
    }
  }

  if (!(0 <= shift && shift <= int(bit_depth))) { throw std::runtime_error("Assertion error"); }// NOLINT
  bit_depth -= uint32_t(shift);

  if (type == 0) {
    std::fill(result.begin(), result.begin() + m_current_block_size.value_or(0), m_input->read_signed_int(bit_depth));
  } else if (type == 1) {
    for (size_t i = 0; i < m_current_block_size.value_or(0); ++i) { result[i] = m_input->read_signed_int(bit_depth); }
  } else if (8 <= type && type <= 12) {
    decode_fixed_prediction_subframe(type - 8, bit_depth, result);
  } else if (32 <= type && type <= 63) {
    decode_linear_predictive_coding_subframe(type - 31, bit_depth, result);
  } else {
    throw DataFormatException("Reserved subframe type");
  }

  if (shift > 0) {
    for (size_t i = 0; i < m_current_block_size.value_or(0); ++i) { result[i] <<= shift; }// NOLINT
  }
}

void FrameDecoder::decode_fixed_prediction_subframe(int64_t pred_order,
  uint32_t bit_depth,
  std::vector<int64_t> &result)
{
  if (bit_depth < 1 || bit_depth > 33) { throw std::invalid_argument("bit_depth is invalid"); }
  if (pred_order < 0 || size_t(pred_order) >= FIXED_PREDICTION_COEFFICIENTS.size()) {
    throw std::invalid_argument("pred_order id invalid");
  }
  if (std::cmp_greater(pred_order, m_current_block_size.value_or(0))) {
    throw DataFormatException("Fixed prediction order exceeds block size ");
  }
  if (result.size() < m_current_block_size.value_or(0)) { throw std::invalid_argument("result size is invalid"); }

  for (size_t i = 0; std::cmp_less(i, pred_order); ++i) { result[i] = m_input->read_signed_int(bit_depth); }
  read_residuals(pred_order, result);
  restore_lpc(result, FIXED_PREDICTION_COEFFICIENTS.at(size_t(pred_order)), bit_depth, 0);
}

void FrameDecoder::decode_linear_predictive_coding_subframe(int64_t lpc_order,
  uint32_t bit_depth,
  std::vector<int64_t> &result)
{
  if (bit_depth < 1 || bit_depth > 33) { throw std::invalid_argument("bit_depth is invalid"); }
  if (lpc_order < 1 || lpc_order > 32) { throw std::invalid_argument("lpc_order is invalid"); }
  if (lpc_order > int(m_current_block_size.value_or(0))) { throw DataFormatException("LPC order exceeds block size"); }
  if (result.size() < size_t(m_current_block_size.value_or(0))) {
    throw std::invalid_argument("result size is invalid");
  }

  for (size_t i = 0; std::cmp_less(i, lpc_order); ++i) { result.at(i) = m_input->read_signed_int(bit_depth); }

  auto precision = m_input->read_uint(4) + 1;
  if (precision == 16) { throw DataFormatException("Invalid LPC precision"); }

  auto shift = m_input->read_signed_int(5);
  if (shift < 0) { throw DataFormatException("Invalid LPC shift"); }

  std::vector<int64_t> coefs(static_cast<size_t>(lpc_order));
  for (auto &coef : coefs) { coef = m_input->read_signed_int(size_t(precision)); }

  read_residuals(lpc_order, result);
  restore_lpc(result, coefs, bit_depth, shift);
}

void FrameDecoder::restore_lpc(std::vector<int64_t> &result,
  const std::vector<int64_t> &coefs,
  uint32_t bit_depth,
  int shift)
{
  if (result.size() < m_current_block_size.value_or(0)) { throw std::invalid_argument("result size is invalid"); }
  if (bit_depth < 1 || bit_depth > 33) { throw std::invalid_argument("bit_depth is invalid"); }
  if (shift < 0 || shift > 63) { throw std::invalid_argument("shift is invalid"); }

  auto lower_bound = (-1) << (bit_depth - 1);// NOLINT
  auto upper_bound = -(lower_bound + 1);

  for (size_t i = coefs.size(); i < m_current_block_size.value_or(0); ++i) {
    int64_t sum = 0;
    for (size_t j = 0; j < coefs.size(); ++j) { sum += result.at(i - 1 - j) * coefs.at(j); }

    assert((sum >> 53) == 0 || (sum >> 53) == -1);// NOLINT
    sum = result.at(i) + (sum >> shift);// NOLINT

    if (sum < lower_bound || sum > upper_bound) { throw DataFormatException("Post-LPC result exceeds bit depth"); }
    result.at(i) = sum;
  }
}

void FrameDecoder::read_residuals(int64_t warmup, std::vector<int64_t> &result)
{
  if (warmup < 0 || std::cmp_greater(warmup, m_current_block_size.value_or(0))) {


    auto method = m_input->read_uint(2);
    if (method >= 2) { throw DataFormatException("Reserved residual coding method"); }
    assert(method == 0 || method == 1);

    const int param_bits = method == 0 ? 4 : 5;
    const int escape_param = method == 0 ? 0xF : 0x1F;

    auto partition_order = m_input->read_uint(4);
    const uint64_t num_partitions = 1U << static_cast<uint8_t>(partition_order);

    if (m_current_block_size.value_or(0) % num_partitions != 0) {
      throw DataFormatException("Block size not divisible by number of Rice partitions");
    }

    for (size_t inc = m_current_block_size.value_or(0) >> partition_order,// NOLINT
      part_end = inc,
                result_index = size_t(warmup);
      part_end <= m_current_block_size.value_or(0);
      part_end += inc) {

      auto param = m_input->read_uint(size_t(param_bits));

      if (param == escape_param) {
        auto num_bits = m_input->read_uint(5);

        for (; result_index < part_end; result_index++) {
          result.at(result_index) = m_input->read_signed_int(size_t(num_bits));
        }
      } else {
        m_input->read_rice_signed_ints(size_t(param), result, result_index, part_end);
        result_index = part_end;
      }
    }
  }
}
}// namespace flac
