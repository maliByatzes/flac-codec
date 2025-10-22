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
  [[nodiscard]] virtual int read_byte() = 0;
  virtual void read_fully(std::vector<uint8_t> &bytes) = 0;

  virtual void reset_crcs() = 0;
  [[nodiscard]] virtual int get_crc8() = 0;
  [[nodiscard]] virtual int get_crc16() = 0;

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

  void check_byte_aligned() const;
  void fill_bit_buffer();
  int read_underlying();
  void update_crcs(int unused_trailing_bytes);

public:
  FlacLowLevelInput();

  [[nodiscard]] long get_position() const override;
  [[nodiscard]] int get_bit_position() const override;

  int read_uint(int nnn) override;
  int read_signed_int(int nnn) override;
  void read_rice_signed_ints(int param, std::vector<long> &result, int start, int end) override;
  int read_byte() override;
  void read_fully(std::vector<uint8_t> &bytes) override;
  void reset_crcs() override;
  [[nodiscard]] int get_crc8() override;
  [[nodiscard]] int get_crc16() override;
  void close() override;

protected:
  void position_changed(long pos);
  virtual int read_underlying(std::vector<uint8_t> &buf, int off, int len) = 0;

private:
  static const int RICE_DECODING_TABLE_BITS = 13;
  static const int RICE_DECODING_TABLE_MASK = (1U << RICE_DECODING_TABLE_BITS) - 1U;// NOLINT
  static std::vector<std::vector<uint8_t>> RICE_DECODING_CONSUMED_TABLES;
  static std::vector<std::vector<int>> RICE_DECODING_VALUE_TABLES;
  static const int RICE_DECODING_CHUNK = 4;

  static void initialize();
};

}// namespace flac
