#pragma once

#include <exception>
#include <stdexcept>
#include <string>
#include <utility>

namespace flac {

class DataFormatException : public std::runtime_error
{
public:
  DataFormatException() : std::runtime_error("") {}

  explicit DataFormatException(const std::string &msg) : std::runtime_error(msg) {}

  DataFormatException(const std::string &msg, std::exception cause) : std::runtime_error(msg), m_cause(std::move(cause))
  {}

  [[nodiscard]] const std::exception &cause() const { return m_cause; }

private:
  std::exception m_cause;
};

}// namespace flac
