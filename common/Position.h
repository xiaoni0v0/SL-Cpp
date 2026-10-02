#pragma once

#include <compare>
#include <string>

// 源码里的一个位置（行、列）
struct Position {
    std::size_t row{0}, col{0};

    std::strong_ordering operator<=>(const Position &) const = default;
};

// 源码里的一个位置（行、列、文件路径）
struct SourceLocation {
    std::string file_path;
    Position pos;
};
