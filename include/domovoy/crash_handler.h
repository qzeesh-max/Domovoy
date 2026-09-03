#pragma once
#if defined(__APPLE__) || defined(__linux__)
#include <signal.h>
#endif

namespace domovoy {
namespace crash {

class CrashHandler {
public:
    static CrashHandler& GetInstance();

    void Install();
    void Uninstall();

private:
    CrashHandler() = default;
    ~CrashHandler() = default;

    bool installed_{false};

#if defined(__APPLE__) || defined(__linux__)
    static void HandleCrashSignal(int sig, siginfo_t* info, void* ucontext);
#elif defined(_WIN32)
    void* veh_handle_{nullptr};
    static long __stdcall VectoredExceptionHandler(struct _EXCEPTION_POINTERS* ep);
#endif
};

} // namespace crash
} // namespace domovoy
