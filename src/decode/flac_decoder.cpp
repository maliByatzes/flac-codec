#include "flac_codec/common/frame_info.h"
#include <cstddef>
#include <flac_codec/common/seek_table.h>
#include <flac_codec/common/stream_info.h>
#include <flac_codec/decode/flac_decoder.h>
#include <flac_codec/decode/seekable_file_flac_input.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace flac {

FlacDecoder::FlacDecoder(const std::string &file_name)
{
  m_input = std::make_unique<SeekableFileFlacInput>(file_name);

  if (static_cast<uint32_t>(m_input->read_uint(32)) != 0x664C6143) { throw std::logic_error("Invalid magic string"); }

  m_metadata_end_pos = std::nullopt;
}

std::optional<std::pair<uint8_t, std::vector<uint8_t>>> FlacDecoder::read_and_handle_metadata_block()
{
  if (m_metadata_end_pos == std::nullopt) { return std::nullopt; }

  bool last = static_cast<int>(m_input->read_uint(1)) != 0;
  auto type = static_cast<uint8_t>(m_input->read_uint(7));
  auto length = static_cast<uint32_t>(m_input->read_uint(24));
  std::vector<uint8_t> data(length);
  m_input->read_fully(data);

  if (static_cast<int>(type) == 0) {
    if (m_stream_info != nullptr) { throw std::logic_error("Duplicate stream info metadata block"); }
    m_stream_info = std::make_unique<StreamInfo>(data);
  } else {
    if (m_stream_info == nullptr) { throw std::logic_error("Expected stream info metadata block"); }
    if (static_cast<int>(type) == 3) {
      if (m_seek_table != nullptr) { throw std::logic_error("Duplicate seek table metadata block"); }
      m_seek_table = std::make_unique<SeekTable>(data);
    }
  }

  if (last) {
    m_metadata_end_pos = m_input->get_position();
    // m_frame_dec = std::make_unique<FrameDecoder>(m_input, m_stream_info->bit_depth);
  }

  return std::make_pair(type, data);
}

uint32_t FlacDecoder::read_audio_block([[maybe_unused]] std::vector<std::vector<uint32_t>> &samples,
  [[maybe_unused]] int off)
{
  /*
  if (m_frame_dec == nullptr) { throw std::logic_error("Metadata blocks not fully consumed yet"); }
  auto frame = m_frame_dec->read_frame(samples, off);
  if (frame == nullptr) {
    return 0;
  } else {
    return frame.block_size;
  }*/
  return 0;
}

uint32_t FlacDecoder::seek_and_read_audio_block(uint64_t pos, Samples &samples, int off)
{
  if (m_frame_dec == nullptr) { throw std::logic_error("Metadata blocks not fully consumed yet"); }

  auto sample_and_file_pos = get_best_seek_point(pos);
  if (pos - sample_and_file_pos[0] > 300'000) {
    sample_and_file_pos = seek_by_sync_and_decode(pos);
    sample_and_file_pos[1] -= m_metadata_end_pos;
  }
  m_input->seek_to(sample_and_file_pos[1] + m_metadata_end_pos);

  uint64_t curr_pos = sample_and_file_pos[0];
  Samples smpl(m_stream_info->m_num_channels, std::vector<uint32_t>(65536));

  while (true) {
    auto frame = m_frame_dec->read_frame(smpl, 0);
    if (frame == nullptr) { return 0; }
    uint64_t next_pos = curr_pos + frame->m_block_size;
    if (next_pos > pos) {
      for (size_t ch = 0; ch < smpl.size(); ++ch) {
        std::copy(
          smpl[ch].begin() + long(pos - curr_pos), smpl[ch].begin() + long(next_pos - pos), samples[ch].begin() + off);
      }
      return static_cast<uint32_t>(next_pos - pos);
    }

    curr_pos = next_pos;
  }
}

std::vector<uint64_t> FlacDecoder::get_best_seek_point(uint64_t pos) const
{
  uint64_t sample_pos = 0;
  uint64_t file_pos = 0;
  if (m_seek_table != nullptr) {
    for (SeekTable::SeekPoint point : m_seek_table->m_points) {
      if (point.m_sample_offset <= pos) {
        sample_pos = point.m_sample_offset;
        file_pos = point.m_file_offset;
      } else {
        break;
      }
    }
  }

  return { sample_pos, file_pos };
}

std::pair<uint64_t, uint64_t> FlacDecoder::seek_by_sync_and_decode(uint64_t pos)
{
  uint64_t start = m_metadata_end_pos.value_or(0);
  uint64_t end = m_input->get_length().value_or(0);

  while (end - start > 100'000) {
    uint64_t mid = (start + end) >> 1U;
    auto offsets = get_next_frame_offsets(mid);
    if (offsets == nullptr || offsets[0] > pos) {
      end = mid;
    } else {
      start = offsets[1];
    }
  }

  return get_next_frame_offsets(start);
}

std::optional<std::pair<uint64_t, uint64_t>> FlacDecoder::get_next_frame_offsets(uint64_t file_pos)
{
  if (file_pos < m_metadata_end_pos || file_pos > m_input->get_length()) {
    throw std::invalid_argument("File position out of bounds");
  }

  while (true) {
    m_input->seek_to(file_pos);

    int state = 0;
    while (true) {
      auto byte = m_input->read_byte();
      if (byte == std::nullopt) {
        return std::nullopt;
      } else if (static_cast<int>(byte) == 0xFF) {
        state = 1;
      } else if (state == 1 && (byte & 0xFE) == 0xF8) {
        break;
      } else {
        state = 0;
      }
    }

    file_pos = m_input->get_position() - 2;
    m_input->seek_to(file_pos);

    try {
      auto frame = FrameInfo::read_frame(m_input);
      return std::make_pair(get_sample_offset(frame), file_pos);
    } catch (const std::exception &e) {// FIXME: Catch only `DataFormatException`
      file_pos += 2;
    }
  }
}

uint64_t FlacDecoder::get_sample_offset(FrameInfo &frame)
{
  if (frame.m_sample_offset.has_value()) {
    return frame.m_sample_offset.value();
  } else if (frame.frame_index.has_value()) {
    return frame.frame_index.value() * m_stream_info->m_max_block_size;
  } else{
    throw std::logic_error("Assertion error");
  }
}

void FlacDecoder::close()
{
  if (m_input != nullptr) {
    m_stream_info.reset(nullptr);
    m_seek_table.reset(nullptr);
    m_frame_dec.reset(nullptr);
    m_input->close();
    m_input.reset(nullptr);
  }
}
}// namespace flac
