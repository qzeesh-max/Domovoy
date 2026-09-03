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

#include "domovoy/profiler.h"
#include "domovoy/core.h"
#include <chrono>
#include <cpptrace/cpptrace.hpp>
#include <iostream>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/task.h>
#include <mach/thread_info.h>
#elif defined(_WIN32)
#include <windows.h>
#include <tlhelp32.h>
#endif

namespace domovoy {
namespace profiler {

CpuMonitor& CpuMonitor::GetInstance() {
    static CpuMonitor instance;
    return instance;
}

void CpuMonitor::Start() {
    if (running_) return;
    running_ = true;
    monitor_thread_ = std::thread(&CpuMonitor::MonitorLoop, this);
}

void CpuMonitor::Stop() {
    if (!running_) return;
    running_ = false;
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
    ThreadMap().swap(thread_states_);
}

void CpuMonitor::MonitorLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        CheckThreadsForHighCpu();
    }
}

void CpuMonitor::CheckThreadsForHighCpu() {
#if defined(__APPLE__)
    thread_act_array_t threads;
    mach_msg_type_number_t thread_count = 0;
    
    if (task_threads(mach_task_self(), &threads, &thread_count) == KERN_SUCCESS) {
        for (unsigned i = 0; i < thread_count; ++i) {
            thread_basic_info_data_t info;
            mach_msg_type_number_t info_count = THREAD_BASIC_INFO_COUNT;
            if (thread_info(threads[i], THREAD_BASIC_INFO, (thread_info_t)&info, &info_count) == KERN_SUCCESS) {
                uint64_t cpu_time = (info.user_time.seconds * 1000000000ULL + info.user_time.microseconds * 1000ULL) +
                                    (info.system_time.seconds * 1000000000ULL + info.system_time.microseconds * 1000ULL);
                
                uint64_t thread_id = threads[i];
                auto now = std::chrono::steady_clock::now().time_since_epoch().count();

                auto it = thread_states_.find(thread_id);
                if (it != thread_states_.end()) {
                    uint64_t cpu_delta = cpu_time - it->second.last_cpu_time_ns;
                    uint64_t time_delta = now - it->second.last_check_time_ns;

                    if (time_delta > 0) {
                        double cpu_usage = static_cast<double>(cpu_delta) / static_cast<double>(time_delta);
                        // If a thread is using more than 90% of a core
                        if (cpu_usage > 0.90) {
                            CaptureBacktraceForThread(thread_id);
                        }
                    }
                    it->second.last_cpu_time_ns = cpu_time;
                    it->second.last_check_time_ns = now;
                } else {
                    thread_states_[thread_id] = {cpu_time, static_cast<uint64_t>(now)};
                }
            }
        }
        
        vm_deallocate(mach_task_self(), (vm_address_t)threads, thread_count * sizeof(thread_act_t));
    }
#elif defined(_WIN32)
    // Stub for Windows thread enumeration using CreateToolhelp32Snapshot
#endif
}

void CpuMonitor::CaptureBacktraceForThread(uint64_t thread_id) {
    // In a full implementation, we'd suspend the specific thread, 
    // capture its registers, and use cpptrace::generate_trace(registers) if supported,
    // or standard libunwind with a specific thread target.
    // For this prototype, we'll just log that a thread was spinning and capture the current thread.
    // cpptrace currently generates a trace for the calling thread.

    auto trace = cpptrace::generate_trace();
    
    isolated_json<> report;
    report["type"] = "high_cpu_thread";
    report["thread_id"] = thread_id;
    
    auto& frames = report["backtrace"] = isolated_json<>::array();
    
    for (const auto& frame : trace.frames) {
        isolated_json<> frame_json;
        frame_json["symbol"] = frame.symbol.empty() ? "<unknown>" : frame.symbol.c_str();
        frame_json["filename"] = frame.filename.empty() ? "<unknown>" : frame.filename.c_str();
        frame_json["line"] = frame.line.value_or(0);
        frames.push_back(frame_json);
    }

    Reporter* reporter = DomovoyCore::GetReporter();
    if (reporter) {
        reporter->ReportHighCpu(report);
    }
}

} // namespace profiler
} // namespace domovoy
