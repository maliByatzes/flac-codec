#pragma once

#include <cstdint>
#include <flac_codec/common/seek_table.h>
#include <flac_codec/common/stream_info.h>
#include <flac_codec/decode/flac_low_level_input.h>
#include <flac_codec/decode/frame_decoder.h>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace flac {

using Samples = std::vector<std::vector<int64_t>>;

class FlacDecoder
{
public:
  std::unique_ptr<StreamInfo> m_stream_info;
  std::unique_ptr<SeekTable> m_seek_table;

  explicit FlacDecoder(const std::string &file_name);

  std::optional<std::pair<uint8_t, std::vector<uint8_t>>> read_and_handle_metadata_block();
  uint32_t read_audio_block(Samples &samples, size_t offset);
  uint32_t seek_and_read_audio_block(uint64_t pos, Samples &samples, size_t offset);

private:
  std::unique_ptr<IFlacLowLevelInput> m_input;
  std::optional<uint64_t> m_metadata_end_pos;
  std::unique_ptr<FrameDecoder> m_frame_dec;

  [[nodiscard]] std::pair<uint64_t, uint64_t> get_best_seek_point(uint64_t pos) const;
  std::pair<uint64_t, uint64_t> seek_by_sync_and_decode(uint64_t pos);
  std::optional<std::pair<uint64_t, uint64_t>> get_next_frame_offsets(uint64_t file_pos);
  [[nodiscard]] uint64_t get_sample_offset(FrameInfo &frame) const;
  void close();
};

}// namespace flac
