#include "executor/Executor.h"

#ifdef _WIN32
#include <windows.h>
#endif

int main(const int argc, char *argv[]) {
#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif

    const Executor executor{argc, argv};

    return executor.run();
}
