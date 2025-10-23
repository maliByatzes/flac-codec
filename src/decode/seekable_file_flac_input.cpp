#include <cstddef>
#include <flac_codec/decode/seekable_file_flac_input.h>
#include <optional>

namespace flac {

SeekableFileFlacInput::SeekableFileFlacInput(const std::string &filename)
  : m_file_stream(filename, std::ios_base::binary)
{}

std::optional<size_t> SeekableFileFlacInput::get_length() const
{
  m_file_stream.unsetf(std::ios::skipws);
  file.seekg(0, std::ios::end);
  auto file_length = file.tellg();
  file.seekg(0, std::ios::beg);
}
}// namespace flac
