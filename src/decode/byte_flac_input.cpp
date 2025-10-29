#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <flac_codec/decode/byte_flac_input.h>
#include <flac_codec/decode/flac_low_level_input.h>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace flac {

ByteFlacInput::ByteFlacInput(std::vector<uint8_t> bytes) : m_data(std::move(bytes)), m_offset(0) {};

size_t ByteFlacInput::get_length() const { return m_data.size(); }

void ByteFlacInput::seek_to(size_t pos)
{
  m_offset = pos;
  position_changed(pos);
}

std::optional<uint64_t> ByteFlacInput::read_underlying(std::vector<uint8_t> &buf, size_t off, size_t len)
{
  if (off > buf.size() || len > buf.size() - off) {
    const std::string msg{ "off= " + std::to_string(off) + ", buf.size()= " + std::to_string(buf.size())
                           + ", len= " + std::to_string(len)
                           + ", offset is greater than buf.size() OR len is greater than buf.size() - offset (array index is out of bounds)" };
    throw std::invalid_argument(msg);
  }

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
