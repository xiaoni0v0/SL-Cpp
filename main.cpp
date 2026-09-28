#include "executor/Executor.h"

#ifdef _WIN32
#include <windows.h>
#endif

int main(const int argc, char *argv[]) {
#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    // 未处理异常直接结束进程，不经过 WER，退出码保持原异常码
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    SetUnhandledExceptionFilter([](EXCEPTION_POINTERS *exceptionPointers) -> LONG {
        TerminateProcess(GetCurrentProcess(), exceptionPointers->ExceptionRecord->ExceptionCode);
        return EXCEPTION_EXECUTE_HANDLER;
    });
    // 为主线程额外预留栈空间，保证栈溢出后过滤器仍有空间运行
    ULONG stackGuarantee = 64 * 1024;
    SetThreadStackGuarantee(&stackGuarantee);
#endif

    const Executor executor{argc, argv};

    return executor.run();
}
