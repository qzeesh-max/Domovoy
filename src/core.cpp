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

#include "domovoy/core.h"
#include "domovoy/memory.h"
#include "domovoy/profiler.h"
#include "domovoy/io_monitor.h"
#include "domovoy/crash_handler.h"
#include <iostream>

namespace domovoy {

Reporter* DomovoyCore::reporter_ = nullptr;
DomovoyConfig DomovoyCore::config_;
bool DomovoyCore::initialized_ = false;

void DomovoyCore::Init(const DomovoyConfig& config) {
    if (initialized_) {
        return;
    }
    
    config_ = config;

    // 1. Initialize the isolated memory allocator first!
    memory::IsolatedAllocator::GetInstance().Initialize(config.allocator_capacity_bytes);

    // 2. Setup reporter using isolated memory
    isolated_string out_dir(config.output_dir);
    // Use placement new inside our isolated allocator block for the reporter itself.
    // To keep it simple, we allocate it via our StlAllocator proxy or a raw Allocate call.
    void* rep_mem = memory::IsolatedAllocator::GetInstance().Allocate(sizeof(JsonFileReporter));
    reporter_ = new (rep_mem) JsonFileReporter(out_dir);

    // Initialize Crash Handler
    if (config.enable_crash_handler) {
        crash::CrashHandler::GetInstance().Install();
    }

    if (config.enable_io_monitoring) {
        io::IoMonitor::GetInstance().Enable();
    }

    if (config.enable_cpu_profiler) {
        profiler::CpuMonitor::GetInstance().Start();
    }

    // TODO: Setup Leak/Corruption interceptors or walkers
    if (config.enable_leak_detection || config.enable_heap_corruption_detection) {
        // memory::InitializeMemoryTracking();
    }

    initialized_ = true;
}

void DomovoyCore::Shutdown() {
    if (!initialized_) return;

    crash::CrashHandler::GetInstance().Uninstall();
    profiler::CpuMonitor::GetInstance().Stop();

    // Run Leak Detection (Reachability Analysis)
    // In a real framework, we'd check config.enable_leak_detection, but for now we assume it's true
    // if initialized. We need to store config in Core or just run it.
    memory::MemoryAnalyzer::GetInstance().RunLeakDetection();

    // Clean up reporter (explicit destructor call since it was placement new'd)
    io::IoMonitor::GetInstance().ReportLeakedFds();
    io::IoMonitor::GetInstance().Disable();

    if (reporter_) {
        reporter_->~Reporter();
        reporter_ = nullptr;
    }

    // Shut down isolated allocator
    memory::IsolatedAllocator::GetInstance().Shutdown();
    
    initialized_ = false;
}

Reporter* DomovoyCore::GetReporter() {
    return reporter_;
}

const DomovoyConfig& DomovoyCore::GetConfig() {
    return config_;
}

} // namespace domovoy
