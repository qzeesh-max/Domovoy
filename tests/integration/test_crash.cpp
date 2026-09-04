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
#include "test_utils.h"

// Disable ASAN/UBSAN on this specific function so it actually crashes with SIGSEGV
// instead of getting caught by sanitizers (if they were enabled for the test target)
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
__attribute__((no_sanitize("address")))
#endif
#endif
void CauseCrash() {
    volatile int* p = nullptr;
    *p = 42; // Causes SIGSEGV
}

TEST(IntegrationTest, CrashHandler) {
    CleanUpOldReports();
    
    // In a real framework, catching a crash means the process exits after reporting.
    // For testing, we would usually run this in a separate process or EXPECT_DEATH.
    // We'll use Google Test's EXPECT_DEATH which forks on POSIX.
    
    EXPECT_DEATH({
#if defined(_WIN32)
        ::testing::GTEST_FLAG(catch_exceptions) = false;
#endif
        domovoy::DomovoyConfig config;
        config.output_dir = ".";
        domovoy::DomovoyCore::Init(config);
        
        CauseCrash();
        
        domovoy::DomovoyCore::Shutdown();
    }, "");

#if !defined(_WIN32)
    nlohmann::json report = FindAndParseReport("domovoy_crash");
    ASSERT_FALSE(report.is_null()) << "Crash report was not generated!";
    ASSERT_EQ(report["type"], "crash");
#endif
}
