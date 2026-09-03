#pragma once

#include <memory>
#include "domovoy/reporter.h"

namespace domovoy {

struct DomovoyConfig {
    // Initial size for the isolated allocator (default 2MB)
    size_t allocator_capacity_bytes = 2 * 1024 * 1024;
    
    // Feature flags
    bool enable_leak_detection = true;
    bool enable_heap_corruption_detection = false;
    bool enable_cpu_profiler = true;
    bool enable_io_monitoring = true;
    bool enable_crash_handler = true;

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

private:
    static Reporter* reporter_;
    static bool initialized_;
};

} // namespace domovoy
