#include <cstddef>
#include <cstdint>
#include <flac_codec/decode/flac_low_level_input.h>
#include <flac_codec/decode/seekable_file_flac_input.h>
#include <ios>
#include <optional>
#include <string>
#include <vector>

namespace flac {

SeekableFileFlacInput::SeekableFileFlacInput(const std::string &filename)
  : m_file_stream(filename, std::ios_base::binary)
{
  m_file_stream.unsetf(std::ios::skipws);
  m_file_stream.seekg(0, std::ios::end);
  auto file_length = m_file_stream.tellg();
  m_file_stream.seekg(0, std::ios::beg);

  m_file_length = static_cast<size_t>(file_length);
}

size_t SeekableFileFlacInput::get_length() const { return m_file_length; }

void SeekableFileFlacInput::seek_to(size_t pos)
{
  m_file_stream.seekg(static_cast<long>(pos), std::ios::cur);
  position_changed(pos);
}

std::optional<uint64_t> SeekableFileFlacInput::read_underlying(std::vector<uint8_t> &buf, size_t off, size_t len)
{
  buf.resize(m_file_length);
  m_file_stream.seekg(static_cast<long>(off), std::ios::cur);
  m_file_stream.read(reinterpret_cast<char *>(buf.data()), static_cast<long>(len));
  return m_file_stream.gcount();
}

void SeekableFileFlacInput::close()
{
  if (m_file_stream.is_open()) {
    m_file_stream.close();
    FlacLowLevelInput::close();
  }
}
}// namespace flac
