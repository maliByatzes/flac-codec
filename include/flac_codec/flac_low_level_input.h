#pragma once

#include <cstdint>
#include <vector>

namespace flac {

class FlacLowLevelInput// NOLINT
{
public:
  virtual ~FlacLowLevelInput() = default;

  [[nodiscard]] virtual uint64_t get_length() const = 0;
  [[nodiscard]] virtual uint64_t get_position() const = 0;
  [[nodiscard]] virtual uint8_t get_bit_position() const = 0;
  virtual void seek_to(uint64_t pos) = 0;

  virtual int32_t read_uint(int nidx) = 0;
  virtual int32_t read_signed_int(int nidx) = 0;
  virtual void read_rice_signed_ints(int param, std::vector<uint64_t> &result, int start, int end) = 0;
  [[nodiscard]] virtual int32_t read_byte() const = 0;
  virtual void read_fully(const std::vector<uint8_t> &bytes) const = 0;

  virtual void reset_crcs() = 0;
  [[nodiscard]] virtual int get_crc8() const = 0;
  [[nodiscard]] virtual int get_crc16() const = 0;

  virtual void close() = 0;
};

}// namespace flac
