#include "domovoy/io_monitor.h"
#include "domovoy/core.h"
#include <cstdio>    // snprintf — portable on all platforms
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>

namespace domovoy {
namespace io {

#if defined(_WIN32)
#include <windows.h>
#include <detours.h>

static HANDLE(WINAPI *TrueCreateFileA)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE) = CreateFileA;
static HANDLE(WINAPI *TrueCreateFileW)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE) = CreateFileW;
static BOOL(WINAPI *TrueCloseHandle)(HANDLE) = CloseHandle;

HANDLE WINAPI HookedCreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    HANDLE h = TrueCreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    if (h != INVALID_HANDLE_VALUE) {
        IoMonitor::GetInstance().OnOpen((int)(intptr_t)h, lpFileName);
    }
    return h;
}

HANDLE WINAPI HookedCreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    HANDLE h = TrueCreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    if (h != INVALID_HANDLE_VALUE) {
        // Just storing "<wide path>" for simplicity in prototype
        IoMonitor::GetInstance().OnOpen((int)(intptr_t)h, "<wide path>");
    }
    return h;
}

BOOL WINAPI HookedCloseHandle(HANDLE hObject) {
    IoMonitor::GetInstance().OnClose((int)(intptr_t)hObject);
    return TrueCloseHandle(hObject);
}
#endif

IoMonitor& IoMonitor::GetInstance() {
    static IoMonitor instance;
    return instance;
}

void IoMonitor::Enable() {
    if (enabled_) return;
    enabled_ = true;

#if defined(_WIN32)
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)TrueCreateFileA, HookedCreateFileA);
    DetourAttach(&(PVOID&)TrueCreateFileW, HookedCreateFileW);
    DetourAttach(&(PVOID&)TrueCloseHandle, HookedCloseHandle);
    DetourTransactionCommit();
#endif
}

void IoMonitor::Disable() {
    if (!enabled_) return;
    enabled_ = false;

#if defined(_WIN32)
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&(PVOID&)TrueCreateFileA, HookedCreateFileA);
    DetourDetach(&(PVOID&)TrueCreateFileW, HookedCreateFileW);
    DetourDetach(&(PVOID&)TrueCloseHandle, HookedCloseHandle);
    DetourTransactionCommit();
#endif
    std::lock_guard<std::mutex> lock(mutex_);
    FdMap().swap(open_fds_); // Clean up map memory using isolated allocator correctly
}

bool IoMonitor::IsEnabled() const {
    return enabled_;
}

void IoMonitor::OnOpen(int fd, const char* path) {
    if (!enabled_ || fd < 0) return;
    std::lock_guard<std::mutex> lock(mutex_);
    FdInfo info;
    info.is_socket = false;
    if (path) {
        snprintf(info.path, sizeof(info.path), "%s", path);
    } else {
        info.path[0] = '\0';
    }
    open_fds_[fd] = info;
}

void IoMonitor::OnClose(int fd) {
    if (!enabled_ || fd < 0) return;
    std::lock_guard<std::mutex> lock(mutex_);
    open_fds_.erase(fd);
}

void IoMonitor::OnSocket(int fd, int domain, int type, int protocol) {
    if (!enabled_ || fd < 0) return;
    std::lock_guard<std::mutex> lock(mutex_);
    FdInfo info;
    info.is_socket = true;
    snprintf(info.path, sizeof(info.path), "%s", "socket");
    open_fds_[fd] = info;
}

void IoMonitor::ReportLeakedFds() {
    if (!enabled_) return;
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (open_fds_.empty()) return;

    isolated_json<> report;
    report["type"] = "fd_leak";
    
    auto& fds = report["fds"] = isolated_json<>::array();
    
    for (const auto& [fd, info] : open_fds_) {
        isolated_json<> fd_json;
        fd_json["fd"] = fd;
        fd_json["path"] = info.path;
        fd_json["is_socket"] = info.is_socket;
        fds.push_back(fd_json);
    }

    Reporter* reporter = DomovoyCore::GetReporter();
    if (reporter) {
        reporter->ReportIoLeak(report);
    }
}

} // namespace io
} // namespace domovoy

// Platform specific hooking
#if defined(__APPLE__)

#define DYLD_INTERPOSE(_replacement,_replacee) \
   __attribute__((used)) static struct{ const void* replacement; const void* replacee; } _interpose_##_replacee \
            __attribute__ ((section ("__DATA,__interpose"))) = { (const void*)(unsigned long)&_replacement, (const void*)(unsigned long)&_replacee };

extern "C" {

int my_open(const char* path, int oflag, ...) {
    mode_t mode = 0;
    if (oflag & O_CREAT) {
        va_list args;
        va_start(args, oflag);
        mode = va_arg(args, int);
        va_end(args);
    }
    
    int fd = open(path, oflag, mode);
    if (fd >= 0) {
        domovoy::io::IoMonitor::GetInstance().OnOpen(fd, path);
    }
    return fd;
}
DYLD_INTERPOSE(my_open, open);

int my_close(int fd) {
    domovoy::io::IoMonitor::GetInstance().OnClose(fd);
    return close(fd);
}
DYLD_INTERPOSE(my_close, close);

int my_socket(int domain, int type, int protocol) {
    int fd = socket(domain, type, protocol);
    if (fd >= 0) {
        domovoy::io::IoMonitor::GetInstance().OnSocket(fd, domain, type, protocol);
    }
    return fd;
}
DYLD_INTERPOSE(my_socket, socket);

} // extern "C"

#elif defined(_WIN32)
// Stub for Detours
#endif
