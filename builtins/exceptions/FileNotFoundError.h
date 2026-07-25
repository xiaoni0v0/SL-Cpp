#pragma once

#include "SLException.h"

#include <format>
#include <string>

class FileNotFoundError : public SLException {
  public:
    explicit FileNotFoundError(const std::string &message)
        : SLException{
              std::format("FileNotFoundError: No such file or directory: \"{}\"", message)
          } {}
};
