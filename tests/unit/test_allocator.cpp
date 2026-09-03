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
#include "domovoy/allocator.h"
#include <vector>

using namespace domovoy::memory;

TEST(AllocatorTest, InitializeAndAllocate) {
    IsolatedAllocator::GetInstance().Initialize(1024 * 1024); // 1MB
    void* ptr = IsolatedAllocator::GetInstance().Allocate(128);
    EXPECT_NE(ptr, nullptr);
}

TEST(AllocatorTest, StlAllocatorVector) {
    std::vector<int, StlAllocator<int>> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[2], 3);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    IsolatedAllocator::GetInstance().Shutdown();
    return ret;
}
