#pragma once

#include <stdexcept>


class SLException : public std::runtime_error {
public:
    explicit SLException(const std::string &message) : std::runtime_error(message) {
    }
};
