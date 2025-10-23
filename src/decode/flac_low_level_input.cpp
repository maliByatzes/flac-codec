#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <flac_codec/decode/flac_low_level_input.h>
#include <optional>
#include <stdexcept>
#include <sys/types.h>
#include <utility>
#include <vector>

namespace flac {

FlacLowLevelInput::FlacLowLevelInput()// NOLINT
{
  m_byte_buffer.reserve(4096);
  position_changed(0);
  initialize_tables();
  initialize_crcs();
}

std::optional<size_t> FlacLowLevelInput::get_position() const
{
  return (m_byte_buffer_start_pos + m_byte_buffer_index - (m_bit_buffer_len + 7U)) / 8U;
}

std::optional<size_t> FlacLowLevelInput::get_bit_position() const
{
  return (-static_cast<int64_t>(m_bit_buffer_len)) & 7U;// NOLINT
}

void FlacLowLevelInput::position_changed(size_t pos)
{
  m_byte_buffer_start_pos = pos;
  std::ranges::fill(m_byte_buffer, 0);
  m_byte_buffer_len = 0;
  m_byte_buffer_index = 0;
  m_bit_buffer = 0;
  m_bit_buffer_len = 0;
  // reset_crcs(); FIXME: comment out 4 now
}

void FlacLowLevelInput::check_byte_aligned() const
{
  if (m_bit_buffer_len % 8 != 0) { throw std::runtime_error("Not at a byte boundary"); }
}

int64_t FlacLowLevelInput::read_uint(size_t num_of_bits)
{
  if (num_of_bits > 32) { throw std::invalid_argument("Invalid argument, nidx is less than 0 or greater than 32"); }


  while (m_bit_buffer_len < num_of_bits) {
    auto tbyte = read_underlying();
    if (tbyte == std::nullopt) { throw std::runtime_error("Reached EOF"); }
    auto byte = tbyte.value();

    m_bit_buffer = (m_bit_buffer << 8U) | byte;
    m_bit_buffer_len += 8U;
    assert(m_bit_buffer_len <= 64U);
  }

  // NOTE: okay this might actually make the program go 💥
  auto result = (m_bit_buffer) >> (m_bit_buffer_len - num_of_bits);// NOLINT
  if (num_of_bits != 32) {
    result &= (1U << num_of_bits) - 1U;
    assert((result >> num_of_bits) == 0U);
    return static_cast<uint32_t>(result);
  } else {
    m_bit_buffer_len -= num_of_bits;
    assert(m_bit_buffer_len && m_bit_buffer_len <= 64U);
    return static_cast<int32_t>(result);
  }
}

// FIXME: this function is just the action and a disarter waiting to happen
int32_t FlacLowLevelInput::read_signed_int(size_t num_of_bits)
{
  auto shift = 32U - num_of_bits;
  // NOTE: another imminent 💥
  return static_cast<int32_t>((static_cast<uint64_t>(read_uint(num_of_bits)) << shift) >> shift);
}

void FlacLowLevelInput::read_rice_signed_ints(size_t param, std::vector<int64_t> &result, size_t start, size_t end)
{
  if (param > 31) { throw std::invalid_argument("Invalid argument"); }

  auto unary_limit = 1UL << (53U - param);

  auto &consume_table = RICE_DECODING_CONSUMED_TABLES.at(param);
  auto &value_table = RICE_DECODING_VALUE_TABLES.at(param);

  while (true) {
    while (start <= end - RICE_DECODING_CHUNK) {
      if (m_bit_buffer_len < RICE_DECODING_CHUNK * RICE_DECODING_TABLE_BITS) {
        if (ssize_t(m_byte_buffer_index) <= m_byte_buffer_len - 8) {
          fill_bit_buffer();
        } else {
          break;
        }
      }

      for (size_t i = 0; i < RICE_DECODING_CHUNK; i++, start++) {
        auto extracted_bits =
          (m_bit_buffer >> (m_bit_buffer_len - RICE_DECODING_TABLE_BITS)) & RICE_DECODING_TABLE_MASK;
        auto consumed = consume_table.at(extracted_bits);
        if (static_cast<int>(consumed) == 0) { goto middle; }
        m_bit_buffer_len -= static_cast<size_t>(consumed);
        result.at(start) = value_table.at(extracted_bits);
      }
    }

  middle:
    if (start >= end) { break; }
    size_t val = 0;
    while (read_uint(1) == 0) {
      if (val >= unary_limit) { throw std::logic_error("Residual value is too large"); }
      val++;
    }
    val = (val << param) | static_cast<uint32_t>(read_uint(param));
    assert((val >> 53U) == 0);
    val = (val >> 1U) ^ -(val & 1U);// FIXME: this is could be wrong btw
    // assert((val >> 52) == 0 || (val >> 52) == -1);
    result.at(start) = static_cast<int64_t>(val);
    start++;
  }
}

void FlacLowLevelInput::fill_bit_buffer()
{
  auto iidx = m_byte_buffer_index;
  auto nidx = std::min((64 - m_bit_buffer_len) >> 3U, size_t(m_byte_buffer_len) - iidx);
  auto bytes = m_byte_buffer;

  if (nidx > 0) {
    for (size_t jidx = 0; jidx < nidx; jidx++, iidx++) {
      m_bit_buffer = (m_bit_buffer << 8U) | (bytes.at(iidx) & 0xFFU);
    }
    m_bit_buffer_len += nidx << 3U;
  } else if (m_bit_buffer_len <= 56) {
    auto ttemp = read_underlying();
    if (ttemp == std::nullopt) { throw std::runtime_error("Reached EOF"); }
    auto temp = ttemp.value();
    m_bit_buffer = (m_bit_buffer << 8U) | temp;
    m_bit_buffer_len += 8;
  }

  assert(8 <= m_bit_buffer_len && m_bit_buffer_len <= 64);
  m_byte_buffer_index += nidx;
}

std::optional<uint8_t> FlacLowLevelInput::read_byte()
{
  check_byte_aligned();
  if (m_bit_buffer_len >= 8) {
    return static_cast<uint8_t>(read_uint(8));
  } else {
    assert(m_bit_buffer_len == 0);
    return read_underlying();
  }
}

void FlacLowLevelInput::read_fully(std::vector<uint8_t> &bytes)
{
  if (bytes.data() == nullptr) { throw std::invalid_argument("Bytes argument is invalid"); }
  check_byte_aligned();
  for (auto &byte : bytes) { byte = static_cast<uint8_t>(read_uint(8)); }
}

std::optional<uint8_t> FlacLowLevelInput::read_underlying()
{
  if (std::cmp_greater_equal(ssize_t(m_byte_buffer_index), m_byte_buffer_len)) {
    if (m_byte_buffer_len == -1) { return std::nullopt; }
    m_byte_buffer_start_pos += size_t(m_byte_buffer_len);
    update_crcs(0);
    auto res = read_underlying(m_byte_buffer, 0, m_byte_buffer.size());
    if (res == std::nullopt) { return res; }
    auto result = res.value();
    m_byte_buffer_len = ssize_t(result);
    m_crc_start_index = 0;
    if (m_byte_buffer_len <= 0) { return std::nullopt; }
    m_byte_buffer_index = 0;
  }

  assert(std::cmp_less(ssize_t(m_byte_buffer_index), m_byte_buffer_len));
  auto temp = m_byte_buffer.at(m_byte_buffer_index) & 0xFFU;
  m_byte_buffer_index++;
  return temp;
}

void FlacLowLevelInput::reset_crcs()
{
  check_byte_aligned();
  m_crc_start_index = (m_byte_buffer_index - m_bit_buffer_len) / 8U;
  m_crc8 = 0;
  m_crc16 = 0;
}

uint8_t FlacLowLevelInput::get_crc8()
{
  check_byte_aligned();
  update_crcs(m_bit_buffer_len / 8);
  if ((m_crc8 >> 8U) != 0) { throw std::logic_error("Assertion error"); }
  return m_crc8;
}

uint16_t FlacLowLevelInput::get_crc16()
{
  check_byte_aligned();
  update_crcs(m_bit_buffer_len / 8);
  if ((m_crc16 >> 16U) != 0) { throw std::logic_error("Assertion error"); }
  return m_crc16;
}

void FlacLowLevelInput::update_crcs(size_t unused_trailing_bytes)
{
  auto end = m_byte_buffer_index - unused_trailing_bytes;
  for (size_t i = m_crc_start_index; i < end; ++i) {
    auto byte = m_byte_buffer.at(i) & 0xFFU;
    m_crc8 = CRC8_TABLE.at(m_crc8 ^ byte) & 0xFFU;
    m_crc16 = static_cast<uint16_t>(CRC16_TABLE.at((m_crc16 >> 8U) & byte) ^ ((m_crc16 & 0xFFU) << 8U));// NOLINT
    assert((m_crc8 >> 8) == 0);// NOLINT
    assert((m_crc16 >> 16) == 0);// NOLINT
  }
  m_crc_start_index = end;
}

void FlacLowLevelInput::close()
{
  m_byte_buffer.clear();
  m_byte_buffer_len = -1;
  m_byte_buffer_index = 0;
  m_bit_buffer = 0;
  m_bit_buffer_len = 0;
  m_crc8 = 0;
  m_crc16 = 0;
  m_crc_start_index = 0;
}

void FlacLowLevelInput::initialize_tables()
{
  // NOTE: this might soo bad or soo smart 🤷
  RICE_DECODING_CONSUMED_TABLES.assign(31, std::vector<uint8_t>(1U << RICE_DECODING_TABLE_BITS, 0));

  RICE_DECODING_VALUE_TABLES.assign(31, std::vector<uint32_t>(1U << RICE_DECODING_TABLE_BITS, 0));

  for (size_t param = 0; param < RICE_DECODING_CONSUMED_TABLES.size(); ++param) {
    auto consumed = RICE_DECODING_CONSUMED_TABLES.at(param);
    auto values = RICE_DECODING_VALUE_TABLES.at(param);

    for (size_t i = 0;; ++i) {
      auto num_bits = (i >> param) + 1 + param;
      if (num_bits > RICE_DECODING_TABLE_BITS) { break; }
      auto bits = ((1U << param) | (i & ((1U << param) - 1U)));
      auto shift = RICE_DECODING_TABLE_BITS - num_bits;
      for (size_t j = 0; j < (1U << shift); j++) {
        consumed.at((bits << shift) | j) = static_cast<uint8_t>(num_bits);
        values.at((bits << shift) | j) = static_cast<uint32_t>((i >> 1U) ^ -(i & 1U));
      }
    }
    if (consumed.at(0) != 0) { throw std::logic_error("Assertion error"); }
  }
}

void FlacLowLevelInput::initialize_crcs()
{
  CRC8_TABLE.assign(256, 0);
  CRC16_TABLE.assign(256, 0);

  for (size_t i = 0; i < CRC8_TABLE.size(); ++i) {
    auto temp8 = i;
    auto temp16 = i << 8U;
    for (size_t j = 0; j < 8U; ++j) {
      temp8 = (temp8 << 1U) ^ ((temp8 >> 7U) * 0x107U);
      temp16 = (temp16 << 1U) ^ ((temp16 >> 15U) * 0x18005U);
    }
    CRC8_TABLE.at(i) = static_cast<uint8_t>(temp8);
    CRC16_TABLE.at(i) = static_cast<uint16_t>(temp16);
  }
}
}// namespace flac
