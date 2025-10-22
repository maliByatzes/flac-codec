#include <algorithm>
#include <cassert>
#include <flac_codec/flac_low_level_input.h>
#include <stdexcept>

namespace flac {

FlacLowLevelInput::FlacLowLevelInput()
{
  m_byte_buffer.reserve(4096);
  position_changed(0);
  initialize_tables();
  initialize_crcs();
}

long FlacLowLevelInput::get_position() const
{
  return (m_byte_buffer_start_pos + m_byte_buffer_index - (m_bit_buffer_len + 7)) / 8;
}

int FlacLowLevelInput::get_bit_position() const { return (-m_bit_buffer_len) & 7; }// NOLINT

void FlacLowLevelInput::position_changed(long pos)
{
  m_byte_buffer_start_pos = pos;
  std::ranges::fill(m_byte_buffer, 0);
  m_byte_buffer_index = 0;
  m_byte_buffer_index = 0;
  m_bit_buffer = 0;
  m_bit_buffer_len = 0;
  reset_crcs();
}

void FlacLowLevelInput::check_byte_aligned() const
{
  if (m_bit_buffer_len % 8 != 0) { throw std::runtime_error("Not at a byte boundary"); }
}

int FlacLowLevelInput::read_uint(int nnn)
{
  if (nnn < 0 || nnn > 32) { throw std::invalid_argument("Invalid argument, nidx is less than 0 or greater than 32"); }

  while (m_bit_buffer_len < nnn) {
    int byte = read_underlying();
    if (byte == -1) { throw std::runtime_error("Reached EOF"); }

    m_bit_buffer = (m_bit_buffer << 8) | byte;// NOLINT
    m_bit_buffer_len += 8;
    assert(0 <= m_bit_buffer_len && m_bit_buffer_len <= 64);
  }

  // FIXME: this could be wrong
  int result = int(m_bit_buffer) >> (m_bit_buffer_len - nnn);// NOLINT
  if (nnn != 32) {
    result &= (1 << nnn) - 1;// NOLINT
    assert((result >> nnn) == 0);// NOLINT
  }

  m_bit_buffer_len -= nnn;
  assert(0 <= m_bit_buffer_len && m_bit_buffer_len <= 64);

  return result;
}

int FlacLowLevelInput::read_signed_int(int nnn)
{
  int shift = 32 - nnn;
  return (read_uint(nnn) << shift) >> shift;// NOLINT
}

void FlacLowLevelInput::read_rice_signed_ints(int param, std::vector<long> &result, int start, int end)
{
  if (param < 0 || param > 31) { throw std::invalid_argument("Invalid argument"); }

  long unary_limit = 1L << (53 - param);// NOLINT

  std::vector<uint8_t> consume_table = RICE_DECODING_CONSUMED_TABLES.at(size_t(param));
  std::vector<int> value_table = RICE_DECODING_VALUE_TABLES.at(size_t(param));

  while (true) {
    while (start <= end - RICE_DECODING_CHUNK) {
      if (m_bit_buffer_len < RICE_DECODING_CHUNK * RICE_DECODING_TABLE_BITS) {
        if (m_byte_buffer_index <= m_byte_buffer_len - 8) {
          fill_bit_buffer();
        } else {
          break;
        }
      }

      for (int i = 0; i < RICE_DECODING_CHUNK; i++, start++) {
        int extracted_bits =
          static_cast<int>(m_bit_buffer >> (m_bit_buffer_len - RICE_DECODING_TABLE_BITS)) & RICE_DECODING_TABLE_MASK;
        int consumed = static_cast<int>(consume_table.at(static_cast<size_t>(extracted_bits)));
        if (consumed == 0) { goto middle; }
        m_bit_buffer_len -= consumed;
        result.at(size_t(start)) = value_table.at(size_t(extracted_bits));
      }
    }

  middle:
    if (start >= end) { break; }
    long val = 0;
    while (read_uint(1) == 0) {
      if (val >= unary_limit) { throw std::runtime_error("Residual value is too large"); }
      val++;
    }
    // NOLINTBEGIN
    val = (val << param) | read_uint(param);// NOLINT
    assert((val >> 53) == 0);
    val = (val >> 1) ^ -(val & 1);
    assert((val >> 52) == 0 || (val >> 52) == -1);
    result.at(size_t(start)) = val;
    start++;
    // NOLINTEND
  }
}

void FlacLowLevelInput::fill_bit_buffer()
{
  int i = m_byte_buffer_index;// NOLINT
  int n = std::min((64 - m_bit_buffer_len) >> 3, m_byte_buffer_len - i);// NOLINT
  std::vector<uint8_t> bytes = m_byte_buffer;

  if (n > 0) {
    for (int j = 0; j < n; j++, i++) { m_bit_buffer = (m_bit_buffer << 8) | (bytes.at(size_t(i)) & 0xFF); }// NOLINT
    m_bit_buffer_len += n << 3;// NOLINT
  } else if (m_bit_buffer_len <= 56) {
    int temp = read_underlying();
    if (temp == -1) { throw std::runtime_error("Reached EOF"); }
    m_bit_buffer = (m_bit_buffer << 8) | temp;// NOLINT
    m_bit_buffer_len += 8;
  }

  assert(8 <= m_bit_buffer_len && m_bit_buffer_len <= 64);
  m_byte_buffer_index += n;
}

int FlacLowLevelInput::read_byte()
{
  check_byte_aligned();
  if (m_bit_buffer_len >= 8) {
    return read_uint(8);
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

int FlacLowLevelInput::read_underlying()
{
  if (m_byte_buffer_index >= m_byte_buffer_len) {
    if (m_byte_buffer_len == -1) { return -1; }
    m_byte_buffer_start_pos += m_byte_buffer_len;
    update_crcs(0);
    m_byte_buffer_len = read_underlying(m_byte_buffer, 0, int(m_byte_buffer.size()));
    m_crc_start_index = 0;
    if (m_byte_buffer_len <= 0) { return -1; }
    m_byte_buffer_index = 0;
  }

  assert(m_byte_buffer_index < m_byte_buffer_len);
  int temp = m_byte_buffer.at(size_t(m_byte_buffer_index)) & 0xFF;// NOLINT
  m_byte_buffer_index++;
  return temp;
}

void FlacLowLevelInput::reset_crcs()
{
  check_byte_aligned();
  m_crc_start_index = m_byte_buffer_index - m_bit_buffer_len / 8;
  m_crc8 = 0;
  m_crc16 = 0;
}

int FlacLowLevelInput::get_crc8()
{
  check_byte_aligned();
  update_crcs(m_bit_buffer_len / 8);
  if ((m_crc8 >> 8) != 0) { throw std::runtime_error("Assertion error"); }// NOLINT
  return m_crc8;
}

int FlacLowLevelInput::get_crc16()
{
  check_byte_aligned();
  update_crcs(m_bit_buffer_len / 8);
  if ((m_crc16 >> 16) != 0) { throw std::runtime_error("Assertion error"); }// NOLINT
  return m_crc16;
}

void FlacLowLevelInput::update_crcs(int unused_trailing_bytes)
{
  int end = m_byte_buffer_index - unused_trailing_bytes;
  for (int i = m_crc_start_index; i < end; ++i) {
    int b = m_byte_buffer.at(size_t(i)) & 0xFF;// NOLINT
    m_crc8 = CRC8_TABLE.at(size_t(m_crc8 ^ b)) & 0xFF;// NOLINT
    m_crc16 = CRC16_TABLE.at(size_t((m_crc16 >> 8) & b)) ^ ((m_crc16 & 0xFF) << 8);// NOLINT
    assert((m_crc8 >> 8) == 0);// NOLINT
    assert((m_crc16 >> 16) == 0);// NOLINT
  }
  m_crc_start_index = end;
}

void FlacLowLevelInput::close()
{
  m_byte_buffer.clear();
  m_byte_buffer_len = -1;
  m_byte_buffer_index = -1;
  m_bit_buffer = 0;
  m_bit_buffer_len = 0;
  m_crc8 = -1;
  m_crc16 = -1;
  m_crc_start_index = -1;
}

void FlacLowLevelInput::initialize_tables()
{
  // NOTE: this might soo bad or soo smart 🤷
  RICE_DECODING_CONSUMED_TABLES.assign(31, std::vector<uint8_t>(1U << RICE_DECODING_TABLE_BITS, 0));

  RICE_DECODING_VALUE_TABLES.assign(31, std::vector<int>(1U << RICE_DECODING_TABLE_BITS, 0));

  for (int param = 0; param < int(RICE_DECODING_CONSUMED_TABLES.size()); ++param) {
    std::vector<uint8_t> consumed = RICE_DECODING_CONSUMED_TABLES.at(size_t(param));
    std::vector<int> values = RICE_DECODING_VALUE_TABLES.at(size_t(param));

    for (int i = 0;; ++i) {
      int num_bits = (i >> param) + 1 + param;
      if (num_bits > RICE_DECODING_TABLE_BITS) { break; }
      int bits = ((1 << param) | (i & ((1 << param) - 1)));
      int shift = RICE_DECODING_TABLE_BITS - num_bits;
      for (int j = 0; j < (1 << shift); j++) {
        consumed.at(size_t((bits << shift) | j)) = static_cast<uint8_t>(num_bits);
        values.at(size_t((bits << shift) | j)) = (i >> 1) ^ -(i & 1);
      }
    }
    if (consumed.at(0) != 0) { throw std::logic_error("Assertion error"); }
  }
}

void FlacLowLevelInput::initialize_crcs()
{
  CRC8_TABLE.assign(256, 0);
  CRC16_TABLE.assign(256, 0);

  for (int i = 0; i < int(CRC8_TABLE.size()); ++i) {
    int temp8 = i;
    int temp16 = i << 8;
    for (int j = 0; j < 8; ++j) {
      temp8 = (temp8 << 1) ^ ((temp8 >> 7) * 0x107);
      temp16 = (temp16 << 1) ^ ((temp16 >> 15) * 0x18005);
    }
    CRC8_TABLE.at(size_t(i)) = static_cast<uint8_t>(temp8);
    CRC16_TABLE.at(size_t(i)) = static_cast<char>(temp16);
  }
}
}// namespace flac
