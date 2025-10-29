#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <sys/types.h>
#include <vector>

namespace flac {

class IFlacLowLevelInput// NOLINT
{
public:
  virtual ~IFlacLowLevelInput() = default;

  [[nodiscard]] virtual size_t get_length() const = 0;
  [[nodiscard]] virtual size_t get_position() const = 0;
  [[nodiscard]] virtual size_t get_bit_position() const = 0;
  virtual void seek_to(size_t pos) = 0;

  virtual int64_t read_uint(size_t num_of_bits) = 0;
  virtual int32_t read_signed_int(size_t num_of_bits) = 0;
  virtual void read_rice_signed_ints(size_t param, std::vector<int64_t> &result, size_t start, size_t end) = 0;

  [[nodiscard]] virtual std::optional<uint8_t> read_byte() = 0;
  virtual void read_fully(std::vector<uint8_t> &bytes) = 0;

  virtual void reset_crcs() = 0;
  [[nodiscard]] virtual uint8_t get_crc8() = 0;
  [[nodiscard]] virtual uint16_t get_crc16() = 0;

  virtual void close() = 0;
};

class FlacLowLevelInput : public IFlacLowLevelInput
{
private:
  size_t m_byte_buffer_start_pos;
  std::vector<uint8_t> m_byte_buffer;
  std::optional<size_t> m_byte_buffer_len;
  size_t m_byte_buffer_index;

  uint64_t m_bit_buffer;
  size_t m_bit_buffer_len;

  uint8_t m_crc8;
  uint16_t m_crc16;
  std::optional<size_t> m_crc_start_index;

  void check_byte_aligned() const;
  void fill_bit_buffer();
  std::optional<uint8_t> read_underlying();
  void update_crcs(size_t unused_trailing_bytes);

public:
  FlacLowLevelInput();

  [[nodiscard]] size_t get_position() const override;
  [[nodiscard]] size_t get_bit_position() const override;

  int64_t read_uint(size_t num_of_bits) override;
  int32_t read_signed_int(size_t num_of_bits) override;
  void read_rice_signed_ints(size_t param, std::vector<int64_t> &result, size_t start, size_t end) override;
  std::optional<uint8_t> read_byte() override;
  void read_fully(std::vector<uint8_t> &bytes) override;
  void reset_crcs() override;
  [[nodiscard]] uint8_t get_crc8() override;
  [[nodiscard]] uint16_t get_crc16() override;
  void close() override;

protected:
  void position_changed(size_t pos);
  virtual std::optional<uint64_t> read_underlying(std::vector<uint8_t> &buf, size_t off, size_t len) = 0;

private:
  static const size_t RICE_DECODING_TABLE_BITS = 13;
  static const size_t RICE_DECODING_TABLE_MASK = (1U << RICE_DECODING_TABLE_BITS) - 1U;
  static std::vector<std::vector<uint8_t>> RICE_DECODING_CONSUMED_TABLES;
  static std::vector<std::vector<uint32_t>> RICE_DECODING_VALUE_TABLES;
  static const size_t RICE_DECODING_CHUNK = 4;

  static void initialize_tables();

  static std::vector<uint8_t> CRC8_TABLE;
  static std::vector<uint16_t> CRC16_TABLE;

  static void initialize_crcs();
};

}// namespace flac
