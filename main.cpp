#ifdef _WIN32
#include <windows.h>
#endif

#include <filesystem>
#include <iostream>

#include "executor/Executor.h"

int main(const int argc, const char *argv[]) {
#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif

    const Executor executor{argc, argv};

    return executor.run();
}
