#pragma once

#include <string>
#include <vector>

class Executor {
    const std::vector<std::string> argv_;

  public:
    explicit Executor(int argc, const char *argv[]);

    [[nodiscard]] int run() const;
};
