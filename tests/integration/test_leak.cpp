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
