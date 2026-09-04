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

#include <gtest/gtest.h>
#include "domovoy/core.h"
#include <thread>
#include <atomic>
#include <chrono>
#include "test_utils.h"

TEST(IntegrationTest, HighCpu) {
    CleanUpOldReports();
    
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

#if !defined(_WIN32)
    // Verify report was generated (CPU monitor is stubbed on Windows)
    nlohmann::json report = FindAndParseReport("domovoy_cpu");
    ASSERT_FALSE(report.is_null()) << "CPU report was not generated!";
    ASSERT_EQ(report["type"], "cpu_hotspot");
#endif
    
    SUCCEED();
}
