#ifdef _WIN32
#include <windows.h>
#endif

#include <filesystem>
#include <iostream>

#include "executor/Executor.h"


int main(const int argc, char *argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    // 没有输入文件
    if (argc != 2) {
        std::cerr << "Usage: " << std::filesystem::path(argv[0]).filename().string() << " <input_file>" << std::endl;
        return 1;
    }

    const Executor executor{argv[1]};

    return executor.run();
}
