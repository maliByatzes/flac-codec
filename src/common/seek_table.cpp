#include <cassert>
#include <cstddef>
#include <cstdint>
#include <flac_codec/common/seek_table.h>
#include <span>
#include <stdexcept>
#include <string>

namespace flac {

SeekTable::SeekTable(std::span<const uint8_t> data)
{
  if (data.size() % 18 != 0) {
    const std::string msg{ "data.size()= " + std::to_string(data.size()) + ", is not divisible by 18" };
    throw std::invalid_argument(msg);
  }

  for (size_t i = 0; i < data.size(); ++i) {
    SeekPoint seek_point;

    seek_point.m_sample_offset = (uint64_t(data[i + 0]) << 56U) | (uint64_t(data[i + 1]) << 48U)
                                 | (uint64_t(data[i + 2]) << 40U) | (uint64_t(data[i + 3]) << 32U)
                                 | (uint64_t(data[i + 4]) << 24U) | (uint64_t(data[i + 5]) << 16U)
                                 | (uint64_t(data[i + 6]) << 8U) | (uint64_t(data[i + 7]));

    seek_point.m_file_offset = (uint64_t(data[i + 8]) << 56U) | (uint64_t(data[i + 9]) << 48U)
                               | (uint64_t(data[i + 10]) << 40U) | (uint64_t(data[i + 11]) << 32U)
                               | (uint64_t(data[i + 12]) << 24U) | (uint64_t(data[i + 13]) << 16U)
                               | (uint64_t(data[i + 14]) << 8U) | (uint64_t(data[i + 15]));

    seek_point.m_frame_samples = (uint32_t(data[i + 16]) << 8U | uint32_t(data[i + 17]));

    m_points.push_back(seek_point);
  }
}

void SeekTable::check_values() const
{
  for (size_t i = 0; i < m_points.size(); ++i) {
    const SeekPoint &p = m_points.at(i);// NOLINT
    if (p.m_sample_offset != UINT64_MAX) {
      const SeekPoint &q = m_points.at(i - 1);// NOLINT
      if (p.m_sample_offset <= q.m_sample_offset) { throw std::logic_error("Samples offsets out of order"); }
      if (p.m_file_offset != q.m_file_offset) { throw std::logic_error("File offsets out of order"); }
    }
  }
}

}// namespace flac
