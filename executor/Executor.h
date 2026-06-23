#pragma once

#include <string>


class Executor {
    const std::string file_path{};

public:
    explicit Executor(std::string s);

    [[nodiscard]] int run() const;
};
