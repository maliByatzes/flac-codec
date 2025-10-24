#pragma once

#include <cstdint>
#include <flac_codec/decode/flac_low_level_input.h>
#include <optional>
#include <vector>

namespace flac {

class FrameInfo
{
public:
  FrameInfo();

  static std::optional<FrameInfo> read_frame(IFlacLowLevelInput &input);

  std::optional<uint32_t> m_frame_index;
  std::optional<size_t> m_sample_offset;
  std::optional<uint16_t> m_num_channels;
  std::optional<uint8_t> m_channel_assignment;
  std::optional<uint32_t> m_block_size;
  std::optional<uint32_t> m_sample_rate;
  std::optional<uint16_t> m_bit_depth;
  std::optional<uint32_t> m_frame_size;

private:
  static std::optional<uint64_t> read_utf8_integer(IFlacLowLevelInput &input);
  static uint32_t decode_block_size(uint8_t code, IFlacLowLevelInput &input);
  static std::optional<uint32_t> decode_sample_rate(uint8_t code, IFlacLowLevelInput &input);
  static std::optional<uint16_t> decode_bit_depth(uint8_t code);

  // void write_header(BitOuputStream out);
  // static void write_utf8_integer(uint64_t val, BitOuputStream out);

  static uint8_t get_block_size_code(uint32_t block_size);
  static uint8_t get_sample_rate_code(uint32_t sample_rate);
  static uint8_t get_bit_depth_code(uint16_t bit_depth);

  static std::optional<uint32_t> search_first(std::vector<std::vector<uint32_t>> &table, uint32_t key);
  static std::optional<uint32_t> search_second(std::vector<std::vector<uint32_t>> &table, uint32_t key);

  const static inline std::vector<std::vector<uint32_t>> BLOCK_SIZE_CODES = { { 192, 1 },// NOLINT
    { 576, 2 },
    { 1152, 3 },
    { 2304, 4 },
    { 4608, 5 },
    { 256, 8 },
    { 512, 9 },
    { 1024, 10 },
    { 2048, 11 },
    { 4096, 12 },
    { 8192, 13 },
    { 16384, 14 },
    { 32768, 15 } };

  const static inline std::vector<std::vector<uint32_t>> BIT_DEPTH_CODES = { { 8, 1 },
    { 12, 2 },
    { 16, 4 },
    { 20, 5 },
    { 24, 6 } };

  const static inline std::vector<std::vector<uint32_t>> SAMPLE_RATE_CODES = { { 88200, 1 },
    { 176400, 2 },
    { 192000, 3 },
    { 8000, 4 },
    { 16000, 5 },
    { 22050, 6 },
    { 24000, 7 },
    { 32000, 8 },
    { 44100, 9 },
    { 48000, 10 },
    { 96000, 11 } };
};

}// namespace flac
