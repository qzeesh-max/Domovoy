/*
 * Domovoy — Production Observability Framework
 * Copyright (C) 2026 Domovoy Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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
