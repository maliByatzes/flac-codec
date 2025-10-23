#pragma once

#include <cstddef>
#include <flac_codec/decode/flac_low_level_input.h>
#include <fstream>
#include <optional>

namespace flac {

class SeekableFileFlacInput : public FlacLowLevelInput
{
private:
  std::fstream m_file_stream;

public:
  explicit SeekableFileFlacInput(const std::string &filename);

  std::optional<size_t> get_length() const override;
};

}// namespace flac
