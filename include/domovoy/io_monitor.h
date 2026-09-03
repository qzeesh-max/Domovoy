#pragma once

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include "domovoy/allocator.h"

namespace domovoy {
namespace io {

class IoMonitor {
public:
    static IoMonitor& GetInstance();

    void Enable();
    void Disable();
    bool IsEnabled() const;

    // Callbacks from hooks
    void OnOpen(int fd, const char* path);
    void OnClose(int fd);
    void OnSocket(int fd, int domain, int type, int protocol);

    void ReportLeakedFds();

private:
    IoMonitor() = default;
    ~IoMonitor() = default;

    bool enabled_{false};
    std::mutex mutex_;

    struct FdInfo {
        char path[1024];
        bool is_socket = false;
        // Ideally we'd store a stack trace here. cpptrace::generate_trace() might be too 
        // slow to run on every open(). For this prototype, we'll store basic info.
    };

    using FdMap = std::unordered_map<
        int, 
        FdInfo, 
        std::hash<int>, 
        std::equal_to<int>, 
        memory::StlAllocator<std::pair<const int, FdInfo>>
    >;
    
    FdMap open_fds_;
};

} // namespace io
} // namespace domovoy
