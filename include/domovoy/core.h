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

#include <memory>
#include "domovoy/reporter.h"

namespace domovoy {

struct DomovoyConfig {
    // Initial size for the isolated allocator (default 2MB)
    size_t allocator_capacity_bytes = 2 * 1024 * 1024;
    
    // Feature flags
    bool enable_leak_detection = false;
    bool enable_heap_corruption_detection = false;
    bool enable_cpu_profiler = false;
    bool enable_io_monitoring = false;
    bool enable_crash_handler = false;

    bool enable_leak_report_summary = true;
    bool enable_leak_report_details = true;

    // Output directory for reports
    const char* output_dir = ".";
};

class DomovoyCore {
public:
    // Initialize the framework. Should be called early in main().
    static void Init(const DomovoyConfig& config);
    
    // Shutdown the framework and trigger reports (e.g., memory leaks).
    static void Shutdown();

    // Access to the reporter
    static Reporter* GetReporter();

    // Access to the configuration
    static const DomovoyConfig& GetConfig();

private:
    static Reporter* reporter_;
    static DomovoyConfig config_;
    static bool initialized_;
};

} // namespace domovoy
