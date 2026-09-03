#include "file_utils.h"

#include "../cpp_exceptions/FileNotFoundError.h"

#include <fstream>
#include <sstream>

std::string file_read_all(const std::string &file_path) {
    std::ifstream file{file_path};
    if (!file.is_open()) {
        throw FileNotFoundError(file_path);
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    file.close();

    return oss.str();
}
