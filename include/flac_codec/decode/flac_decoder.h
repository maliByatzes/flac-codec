#pragma once

#include <cstdint>
#include <flac_codec/common/seek_table.h>
#include <flac_codec/common/stream_info.h>
#include <flac_codec/decode/flac_low_level_input.h>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace flac {

class FlacDecoder
{
public:
  std::unique_ptr<StreamInfo> m_stream_info;
  std::unique_ptr<SeekTable> m_seek_table;

  explicit FlacDecoder(const std::string &file_name);

  std::optional<std::pair<uint8_t, std::vector<uint8_t>>> read_and_handle_metadata_block();

private:
  std::unique_ptr<IFlacLowLevelInput> m_input;
  std::optional<uint64_t> m_metadata_end_pos;
  // FrameDecoder m_frame_dec;
};

}// namespace flac
