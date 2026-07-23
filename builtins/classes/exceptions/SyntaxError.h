#pragma once

#include "SLException.h"

#include <format>
#include <string>


class SyntaxError : public SLException {
public:
    explicit SyntaxError(const std::string &file_path, const int row, const int col, const std::string &message)
        : SLException{std::format("{}:{}:{}:\nSyntaxError: {}", file_path, row, col, message)} {
    }

    explicit SyntaxError(const std::string &file_path, const std::string &message)
        : SLException{std::format("{}:\nSyntaxError: {}", file_path, message)} {
    }
};
