#include <algorithm>
#include <cassert>
#include <flac_codec/flac_low_level_input.h>
#include <stdexcept>

namespace flac {

FlacLowLevelInput::FlacLowLevelInput()
{
  m_byte_buffer.reserve(4096);
  position_changed(0);
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
}
}// namespace flac
