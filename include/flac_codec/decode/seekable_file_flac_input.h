#pragma once

#include <cstddef>
#include <cstdint>
#include <flac_codec/decode/flac_low_level_input.h>
#include <fstream>
#include <optional>
#include <vector>

namespace flac {

class SeekableFileFlacInput : public FlacLowLevelInput
{
private:
  std::fstream m_file_stream;
  size_t m_file_length;

public:
  explicit SeekableFileFlacInput(const std::string &filename);

  size_t get_length() const override;
  void seek_to(size_t pos) override;
  void close() override;

protected:
  std::optional<uint64_t> read_underlying(std::vector<uint8_t> &buf, size_t off, size_t len) override;
};

}// namespace flac
