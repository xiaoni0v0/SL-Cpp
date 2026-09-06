#pragma once

#include <string>

// 源码位置，文件 + 行 + 列
struct SourceLocation {
    std::string file_path;
    int row{0};
    int col{0};
};
