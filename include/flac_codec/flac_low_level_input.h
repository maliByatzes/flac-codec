#pragma once

#include <cstdint>
#include <vector>

namespace flac {

class IFlacLowLevelInput// NOLINT
{
public:
  virtual ~IFlacLowLevelInput() = default;

  [[nodiscard]] virtual long get_length() const = 0;
  [[nodiscard]] virtual long get_position() const = 0;
  [[nodiscard]] virtual int get_bit_position() const = 0;
  virtual void seek_to(long pos) = 0;

  virtual int read_uint(int nnn) = 0;
  virtual int read_signed_int(int nnn) = 0;
  virtual void read_rice_signed_ints(int param, std::vector<long> &result, int start, int end) = 0;
  [[nodiscard]] virtual int read_byte() const = 0;
  virtual void read_fully(const std::vector<uint8_t> &bytes) const = 0;

  virtual void reset_crcs() = 0;
  [[nodiscard]] virtual int get_crc8() const = 0;
  [[nodiscard]] virtual int get_crc16() const = 0;

  virtual void close() = 0;
};

class FlacLowLevelInput : public IFlacLowLevelInput
{
private:
  long m_byte_buffer_start_pos;
  std::vector<uint8_t> m_byte_buffer;
  int m_byte_buffer_len;
  int m_byte_buffer_index;

  long m_bit_buffer;
  int m_bit_buffer_len;

  int m_crc8;
  int m_crc16;
  int m_crc_start_index;

public:
  FlacLowLevelInput();

  [[nodiscard]] long get_position() const override;
  [[nodiscard]] int get_bit_position() const override;

protected:
  void position_changed(long pos);

private:
  void check_byte_aligned() const;

public:
  int read_uint(int nnn) override;
  int read_signed_int(int nnn) override;
  void read_rice_signed_ints(int param, std::vector<long> &result, int start, int end) override;
};

}// namespace flac
