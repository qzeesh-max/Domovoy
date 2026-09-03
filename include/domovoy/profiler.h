#pragma once

#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <cstdint>
#include "domovoy/allocator.h"

namespace domovoy {
namespace profiler {

class CpuMonitor {
public:
    static CpuMonitor& GetInstance();

    void Start();
    void Stop();

private:
    CpuMonitor() = default;
    ~CpuMonitor() = default;

    void MonitorLoop();

    void CheckThreadsForHighCpu();
    void CaptureBacktraceForThread(uint64_t thread_id);

    std::atomic<bool> running_{false};
    std::thread monitor_thread_;

    // To track CPU usage over time
    struct ThreadState {
        uint64_t last_cpu_time_ns = 0;
        uint64_t last_check_time_ns = 0;
    };

    // StlAllocator used for internal data structures to avoid heap corruption.
    // For a map, we use our isolated allocator.
    using ThreadMap = std::unordered_map<
        uint64_t, 
        ThreadState, 
        std::hash<uint64_t>, 
        std::equal_to<uint64_t>, 
        memory::StlAllocator<std::pair<const uint64_t, ThreadState>>
    >;
    
    ThreadMap thread_states_;
};

} // namespace profiler
} // namespace domovoy
