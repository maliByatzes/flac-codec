#pragma once

#include <flac_codec/decode/flac_low_level_input.h>
#include <optional>
#include <vector>

namespace flac {

class ByteFlacInput : public FlacLowLevelInput
{
private:
  std::vector<uint8_t> m_data;
  size_t m_offset;

public:
  explicit ByteFlacInput(std::vector<uint8_t> bytes);

  [[nodiscard]] size_t get_length() const override;
  void seek_to(size_t pos) override;
  void close() override;

protected:
  std::optional<uint64_t> read_underlying(std::vector<uint8_t> &buf, size_t off, size_t len) override;
};

}// namespace flac
