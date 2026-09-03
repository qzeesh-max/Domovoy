#pragma once
#include <vector>
#include <cstdint>
#include "domovoy/allocator.h"

namespace domovoy {
namespace memory {

struct AllocationRecord {
    void* address;
    size_t size;
    bool marked;
};

// Uses isolated allocator
using AllocationList = std::vector<AllocationRecord, StlAllocator<AllocationRecord>>;

class MemoryAnalyzer {
public:
    static MemoryAnalyzer& GetInstance();

    // Initiates a BDW style reachability analysis
    void RunLeakDetection();

private:
    MemoryAnalyzer() = default;

    // 1. Walk the OS heap and populate allocations
    void EnumerateHeap(AllocationList& allocations);

    // 2. Scan memory segments (BSS, DATA) for pointers
    void ScanDataSegments(AllocationList& allocations);

    // 3. Scan thread stacks for pointers
    void ScanThreadStacks(AllocationList& allocations);

    // Mark and sweep algorithm helpers
    void MarkReachability(AllocationList& allocations, void* ptr_value);
};

} // namespace memory
} // namespace domovoy
