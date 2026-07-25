#pragma once

#include "SLException.h"

#include <format>
#include <string>

class EncodingError : public SLException {
  public:
    explicit EncodingError(
        const std::string &file_path, const size_t pos, const std::string &message
    )
        : SLException{std::format("{}: offset {}:\nEncodingError: {}", file_path, pos, message)} {}
};
