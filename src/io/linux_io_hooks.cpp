#if defined(__linux__)

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <cstdarg>
#include <mutex>
#include <atomic>
#include "domovoy/io_monitor.h"

typedef int (*open_func_t)(const char*, int, ...);
typedef int (*openat_func_t)(int, const char*, int, ...);
typedef int (*close_func_t)(int);
typedef int (*socket_func_t)(int, int, int);

static open_func_t real_open = nullptr;
static open_func_t real_open64 = nullptr;
static openat_func_t real_openat = nullptr;
static openat_func_t real_openat64 = nullptr;
static close_func_t real_close = nullptr;
static socket_func_t real_socket = nullptr;

static std::atomic<bool> io_hooks_initialized{false};
static std::atomic<bool> io_hooks_initializing{false};

static void InitIoHooks() {
    real_open = (open_func_t)dlsym(RTLD_NEXT, "open");
    real_open64 = (open_func_t)dlsym(RTLD_NEXT, "open64");
    real_openat = (openat_func_t)dlsym(RTLD_NEXT, "openat");
    real_openat64 = (openat_func_t)dlsym(RTLD_NEXT, "openat64");
    real_close = (close_func_t)dlsym(RTLD_NEXT, "close");
    real_socket = (socket_func_t)dlsym(RTLD_NEXT, "socket");
}

static inline void EnsureIoInit() {
    if (io_hooks_initialized.load(std::memory_order_acquire)) return;
    if (io_hooks_initializing.exchange(true, std::memory_order_acquire)) {
        while (!io_hooks_initialized.load(std::memory_order_acquire)) {}
        return;
    }
    InitIoHooks();
    io_hooks_initialized.store(true, std::memory_order_release);
}

extern "C" {

int open(const char* path, int oflag, ...) {
    EnsureIoInit();
    mode_t mode = 0;
    if (oflag & O_CREAT) {
        va_list args;
        va_start(args, oflag);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    int fd = real_open(path, oflag, mode);
    if (fd >= 0) {
        domovoy::io::IoMonitor::GetInstance().OnOpen(fd, path);
    }
    return fd;
}

int open64(const char* path, int oflag, ...) {
    EnsureIoInit();
    mode_t mode = 0;
    if (oflag & O_CREAT) {
        va_list args;
        va_start(args, oflag);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    int fd = real_open64 ? real_open64(path, oflag, mode) : real_open(path, oflag, mode);
    if (fd >= 0) {
        domovoy::io::IoMonitor::GetInstance().OnOpen(fd, path);
    }
    return fd;
}

int openat(int dirfd, const char* path, int oflag, ...) {
    EnsureIoInit();
    mode_t mode = 0;
    if (oflag & O_CREAT) {
        va_list args;
        va_start(args, oflag);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    int fd = real_openat(dirfd, path, oflag, mode);
    if (fd >= 0) {
        domovoy::io::IoMonitor::GetInstance().OnOpen(fd, path);
    }
    return fd;
}

int openat64(int dirfd, const char* path, int oflag, ...) {
    EnsureIoInit();
    mode_t mode = 0;
    if (oflag & O_CREAT) {
        va_list args;
        va_start(args, oflag);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    int fd = real_openat64 ? real_openat64(dirfd, path, oflag, mode) : real_openat(dirfd, path, oflag, mode);
    if (fd >= 0) {
        domovoy::io::IoMonitor::GetInstance().OnOpen(fd, path);
    }
    return fd;
}

int close(int fd) {
    EnsureIoInit();
    domovoy::io::IoMonitor::GetInstance().OnClose(fd);
    if (real_close) {
        return real_close(fd);
    }
    return -1;
}

int socket(int domain, int type, int protocol) {
    EnsureIoInit();
    int fd = real_socket(domain, type, protocol);
    if (fd >= 0) {
        domovoy::io::IoMonitor::GetInstance().OnSocket(fd, domain, type, protocol);
    }
    return fd;
}

} // extern "C"

// Dummy function to force linker to include this object file
extern "C" void domovoy_linux_io_hooks_init() {
    EnsureIoInit();
}

#endif // __linux__
