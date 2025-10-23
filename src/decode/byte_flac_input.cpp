#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <flac_codec/decode/byte_flac_input.h>
#include <flac_codec/decode/flac_low_level_input.h>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace flac {

ByteFlacInput::ByteFlacInput(std::vector<uint8_t> bytes) : m_data(std::move(bytes)), m_offset(0) {};

std::optional<size_t> ByteFlacInput::get_length() const { return m_data.size(); }

void ByteFlacInput::seek_to(size_t pos)
{
  m_offset = pos;
  position_changed(pos);
}

std::optional<uint64_t> ByteFlacInput::read_underlying(std::vector<uint8_t> &buf, size_t off, size_t len)
{
  if (off > buf.size() || len > buf.size() - off) { throw std::invalid_argument("Buffer is out of bounds."); }

  auto min = std::min(m_data.size() - m_offset, len);
  if (min == 0) { return std::nullopt; }

  std::copy(m_data.begin() + long(m_offset), m_data.begin() + long(m_offset) + long(min), buf.begin() + long(off));

  m_offset += min;
  return min;
}

void ByteFlacInput::close()
{
  // NOTE: eeehehehe
  if (!m_data.empty()) {
    m_data.clear();
    FlacLowLevelInput::close();
  }
}
}// namespace flac
