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
#include <vector>
#include <iostream>
#include <thread>
#include <cpptrace/cpptrace.hpp>
#include "test_utils.h"

struct BaseClass {
    virtual ~BaseClass() = default;
};

struct DerivedLeakClass : public BaseClass {
    int data = 42;
};

TEST(IntegrationTest, MemoryLeak) {
    CleanUpOldReports();
    
    domovoy::DomovoyConfig config;
    config.output_dir = ".";
    config.enable_leak_detection = true;
    domovoy::DomovoyCore::Init(config);

    // Intentionally leak a polymorphic object
    BaseClass* leaked_obj = new DerivedLeakClass();
    (void)leaked_obj;
    
    // Shutting down Domovoy should generate a report
    domovoy::DomovoyCore::Shutdown();
    
    nlohmann::json report = FindAndParseReport("domovoy_leaks");
    ASSERT_FALSE(report.is_null()) << "Leak report was not generated!";
    ASSERT_EQ(report["type"], "memory_leaks");
    ASSERT_TRUE(report.contains("summary"));
    ASSERT_GE(report["summary"]["total_leaks"].get<int>(), 1);
    
    SUCCEED();
}

TEST(IntegrationTest, MemoryLeakStress) {
    CleanUpOldReports();
    
    domovoy::DomovoyConfig config;
    config.output_dir = ".";
    config.enable_leak_detection = true;
    config.enable_leak_report_details = true;
    domovoy::DomovoyCore::Init(config);

    constexpr int kNumThreads = 8;
    constexpr int kLeaksPerThread = 100;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < kNumThreads; ++i) {
        threads.emplace_back([]() {
            for (int j = 0; j < kLeaksPerThread; ++j) {
                // Intentionally leak a polymorphic object
                BaseClass* leaked_obj = new DerivedLeakClass();
                (void)leaked_obj;
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Shutting down Domovoy should generate a report
    domovoy::DomovoyCore::Shutdown();
    
    nlohmann::json report = FindAndParseReport("domovoy_leaks");
    ASSERT_FALSE(report.is_null()) << "Leak report was not generated!";
    ASSERT_EQ(report["type"], "memory_leaks");
    ASSERT_TRUE(report.contains("summary"));
    
    // We expect at least kNumThreads * kLeaksPerThread leaks. 
    // There could be other background leaks from test framework overhead, so we use >=
    ASSERT_GE(report["summary"]["total_leaks"].get<int>(), kNumThreads * kLeaksPerThread);
    
    // Validate that RTTI extraction correctly resolved the type name for these leaks
    bool found_rtti = false;
    if (report.contains("leaks_by_size")) {
        for (const auto& group : report["leaks_by_size"]) {
            for (const auto& addr_info : group["addresses"]) {
                if (addr_info.contains("type")) {
                    std::string type_name = addr_info["type"].get<std::string>();
                    if (type_name.find("DerivedLeakClass") != std::string::npos) {
                        found_rtti = true;
                        break;
                    }
                }
            }
            if (found_rtti) break;
        }
    }
    
    ASSERT_TRUE(found_rtti) << "Failed to resolve RTTI for leaked objects\nJSON Dump:\n" << report.dump(4);
    
    SUCCEED();
}
