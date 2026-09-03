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
