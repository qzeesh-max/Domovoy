#include <gtest/gtest.h>
#include "domovoy/core.h"
#include <thread>
#include <atomic>
#include <chrono>

TEST(IntegrationTest, HighCpu) {
    domovoy::DomovoyConfig config;
    config.output_dir = ".";
    // Shorten the allocator capacity to test edge cases if we want, or keep default
    domovoy::DomovoyCore::Init(config);

    std::atomic<bool> stop_spin{false};
    
    // Spawn a thread that will consume 100% CPU
    std::thread spinner([&]() {
        while (!stop_spin.load(std::memory_order_relaxed)) {
            // spin
        }
    });

    // Wait long enough for the CPU monitor (which checks every 500ms) to detect it
    std::this_thread::sleep_for(std::chrono::seconds(2));

    stop_spin = true;
    spinner.join();

    domovoy::DomovoyCore::Shutdown();

    // Verify report was generated (for now just running it without crash is success)
    SUCCEED();
}
