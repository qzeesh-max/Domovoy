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
