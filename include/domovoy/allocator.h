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

#include <cstddef>
#include <cstdint>
#include <new>
#include <limits>
#include <atomic>
#include <mutex>

namespace domovoy {
namespace memory {

// A highly simplified cache/TLB friendly slab allocator for Domovoy's internal use.
// Maps a large region of memory directly from the OS (mmap/VirtualAlloc) to avoid 
// the C-runtime heap. This ensures resilience against application heap corruption.
class IsolatedAllocator {
public:
    static IsolatedAllocator& GetInstance();

    // Initialize with a specific capacity. Must be called during Domovoy::Init().
    void Initialize(size_t initial_capacity_bytes);

    // Allocate memory from the isolated pool. Thread-safe.
    void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t));

    // Free memory back to the isolated pool. Thread-safe.
    void Deallocate(void* ptr, size_t size);

    // Tear down the allocator and unmap memory.
    void Shutdown();

private:
    IsolatedAllocator() = default;
    ~IsolatedAllocator() = default;

    // Disallow copy/move
    IsolatedAllocator(const IsolatedAllocator&) = delete;
    IsolatedAllocator& operator=(const IsolatedAllocator&) = delete;

    struct Block {
        void* memory;
        size_t size;
        size_t used;
        Block* next;
    };

    Block* AllocateOSBlock(size_t size);
    void FreeOSBlock(Block* block);

    std::mutex mutex_;
    Block* head_block_{nullptr};
    
    // For a production allocator, we would use proper free lists (slabs) for common sizes.
    // To keep this MVP robust, we will use a synchronized bump allocator with 
    // a basic free-list for recycled nodes of specific sizes if needed, or just 
    // rely on bump-allocating OS blocks and never truly freeing until shutdown 
    // for objects that live for the lifetime of the process.
    // We'll implement a simple thread-safe bump allocator across linked OS blocks.
};

// STL compatible allocator using IsolatedAllocator
template <typename T>
class StlAllocator {
public:
    using value_type = T;

    StlAllocator() = default;
    
    template <typename U>
    constexpr StlAllocator(const StlAllocator<U>&) noexcept {}

    [[nodiscard]] T* allocate(std::size_t n) {
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::bad_alloc();
        }
        if (auto p = static_cast<T*>(IsolatedAllocator::GetInstance().Allocate(n * sizeof(T), alignof(T)))) {
            return p;
        }
        throw std::bad_alloc();
    }

    void deallocate(T* p, std::size_t n) noexcept {
        IsolatedAllocator::GetInstance().Deallocate(p, n * sizeof(T));
    }
};

template <typename T, typename U>
bool operator==(const StlAllocator<T>&, const StlAllocator<U>&) { return true; }

template <typename T, typename U>
bool operator!=(const StlAllocator<T>&, const StlAllocator<U>&) { return false; }

} // namespace memory
} // namespace domovoy
