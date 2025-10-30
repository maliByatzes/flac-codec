#include <cstdint>
#include <exception>
#include <flac_codec/common/frame_info.h>
#include <flac_codec/common/stream_info.h>
#include <flac_codec/decode/byte_flac_input.h>
#include <flac_codec/decode/data_format_exception.h>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace flac {

StreamInfo::StreamInfo(const std::vector<uint8_t> &bytes)
{
  if (bytes.size() != 34) {
    const std::string msg{ "bytes.size()= " + std::to_string(bytes.size()) + ", des not equal 34" };
    throw std::invalid_argument(msg);
  }

  try {
    auto input = ByteFlacInput(bytes);
    m_min_block_size = static_cast<uint16_t>(input.read_uint(16));
    m_max_block_size = static_cast<uint16_t>(input.read_uint(16));
    m_min_frame_size = static_cast<uint32_t>(input.read_uint(24));
    m_max_frame_size = static_cast<uint32_t>(input.read_uint(24));

    if (m_min_block_size < 16) {
      const std::string msg{ "min_block_size= " + std::to_string(m_min_block_size) + ", is less than 16" };
      throw DataFormatException(msg);
    }

    if (m_max_block_size < m_min_block_size) {
      const std::string msg{ "max_block_size= " + std::to_string(m_max_block_size)
                             + ", is less than min_block_size= " + std::to_string(m_min_block_size) };
      throw DataFormatException(msg);
    }

    if (m_min_frame_size != 0 && m_max_frame_size != 0 && m_max_frame_size < m_min_frame_size) {
      const std::string msg{ "min_frame_size= " + std::to_string(m_min_frame_size)
                             + " is not equal to 0 AND max_frame_size= " + std::to_string(m_max_frame_size)
                             + ", is not eqaul to 0 AND max_frame_size= " + std::to_string(m_max_frame_size)
                             + ", is less than the min_frame_size= " + std::to_string(m_min_frame_size) };
      throw DataFormatException(msg);
    }

    m_sample_rate = static_cast<uint32_t>(input.read_uint(3)) + 1;
    if (m_sample_rate == 0 || m_sample_rate > 655350) {
      const std::string msg{ "sample_rate= " + std::to_string(m_sample_rate)
                             + ", is equal to 0 OR is greater than 655350" };
      throw DataFormatException(msg);
    }

    m_num_channels = static_cast<uint8_t>(input.read_uint(3) + 1);
    m_bit_depth = static_cast<uint16_t>(input.read_uint(5) + 1);
    m_num_samples = static_cast<uint64_t>(input.read_uint(18)) << 18U | static_cast<uint64_t>(input.read_uint(18));
    input.read_fully(m_md5_hash);
  } catch (const std::exception &e) {
    throw std::runtime_error(e.what());
  }
}

void StreamInfo::check_values() const
{
  if ((m_min_block_size >> 16U) != 0) {
    const std::string msg{ "min_block_size= " + std::to_string(m_min_block_size)
                           + ", is not 16 bits (Invalid minimum block size)" };
    throw std::runtime_error(msg);
  }

  if ((m_max_block_size >> 16U) != 0) {

    const std::string msg{ "max_block_size= " + std::to_string(m_max_block_size)
                           + ", is not 16 bits (Invalid maximum block size)" };
    throw std::runtime_error(msg);
  }

  if ((m_min_frame_size >> 24U) != 0) {
    const std::string msg{ "min_frame_size= " + std::to_string(m_min_frame_size)
                           + ", is not 24 bits (Invalid minimum frame size)" };
    throw std::runtime_error(msg);
  }

  if ((m_max_frame_size >> 24U) != 0) {
    const std::string msg{ "max_frame_size= " + std::to_string(m_max_frame_size)
                           + ", is not 24 bits (Invalid maximum frame size)" };
    throw std::runtime_error(msg);
  }

  if (m_sample_rate == 0 || (m_sample_rate >> 20U) != 0) {
    const std::string msg{ "sample_rate= " + std::to_string(m_sample_rate)
                           + ", is equal to 0 OR is not 20 bits (Invalid sample rate)" };

    throw std::runtime_error(msg);
  }

  if (m_num_channels < 1 || m_num_channels > 8) {
    const std::string msg{ "num_channels= " + std::to_string(static_cast<int>(m_num_channels))
                           + ", is less tha 1 OR is greater than 8 (Invalid number of channels)" };
    throw std::runtime_error(msg);
  }

  if (m_bit_depth < 4 || m_bit_depth > 32) {
    const std::string msg{ "bit_depth= " + std::to_string(m_bit_depth)
                           + ", is less than 4 OR greater than 32 (Invalid bit depth)" };
    throw std::runtime_error(msg);
  }

  if ((static_cast<uint64_t>(m_num_samples) >> 36U) != 0) {
    const std::string msg{ "num_samples= " + std::to_string(m_num_samples)
                           + ", is not 36 bits (Invalid number of samples)" };
    throw std::runtime_error(msg);
  }

  if (m_md5_hash.size() != 16) {
    const std::string msg{ "MD5 hash size= " + std::to_string(m_md5_hash.size())
                           + ", is not equal 16 (Invalid MD5 hash length)" };
    throw std::runtime_error(msg);
  }
}

void StreamInfo::check_frame(FrameInfo &meta) const
{
  if (!meta.m_num_channels.has_value() && meta.m_num_channels.value() != m_num_channels) {
    const std::string msg{ "meta.num_channels= " + std::to_string(static_cast<int>(meta.m_num_channels.value()))
                           + ", is not equal num_channels= " + std::to_string(static_cast<int>(m_num_channels))
                           + " (Chanel count mismatch)" };
    throw DataFormatException(msg);
  }

  if (!meta.m_sample_rate.has_value() && meta.m_sample_rate.value() != m_sample_rate) {
    const std::string msg{ "meta.sample_rate= " + std::to_string(meta.m_sample_rate.value())
                           + ", is not equal sample_rate= " + std::to_string(m_sample_rate)
                           + " (Sample rate mismatch)" };
    throw DataFormatException(msg);
  }

  if (!meta.m_bit_depth.has_value() && meta.m_bit_depth.value() != m_bit_depth) {
    const std::string msg{ "meta.bit_depth= " + std::to_string(meta.m_bit_depth.value())
                           + ", is not equal to bit_depth= " + std::to_string(m_bit_depth) + " (Bit depth mismatch)" };
    throw DataFormatException(msg);
  }

  if (m_num_samples != 0 && meta.m_block_size.value_or(0) > m_num_samples) {
    const std::string msg{ "num_samples= " + std::to_string(m_num_samples)
                           + ", meta.block_size= " + std::to_string(meta.m_block_size.value_or(0))
                           + ", Block size exceeds total number of samples" };
    throw DataFormatException(msg);
  }

  if (meta.m_block_size.has_value() && meta.m_block_size.value_or(0) > m_max_block_size) {
    const std::string msg{ "meta.m_block_size= " + std::to_string(meta.m_block_size.value())
                           + ", Block size exceeds maximum block size" };
    throw DataFormatException(msg);

    if (m_min_frame_size != 0 && meta.m_frame_size.value_or(0) < m_min_frame_size) {
      throw DataFormatException("Frame size less than minimum");
    }

    if (m_max_frame_size != 0 && meta.m_frame_size.value_or(0) > m_max_frame_size) {
      throw DataFormatException("Frame size exceeds maximum");
    }
  }
}

}// namespace flac
