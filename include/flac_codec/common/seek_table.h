#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace flac {


class SeekTable
{
public:
  struct SeekPoint
  {
  public:
    uint64_t m_sample_offset{};
    uint64_t m_file_offset{};
    uint32_t m_frame_samples{};

    SeekPoint() = default;
  };

  std::vector<SeekPoint> m_points;

  SeekTable() = default;
  explicit SeekTable(std::span<const uint8_t> data);

  void check_values() const;
  // void write(bool last, BitOutputStream out);
};


}// namespace flac
