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

#include "domovoy/allocator.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <stdexcept>
#include <algorithm>

namespace domovoy {
namespace memory {

IsolatedAllocator& IsolatedAllocator::GetInstance() {
    static IsolatedAllocator instance;
    return instance;
}

void IsolatedAllocator::Initialize(size_t initial_capacity_bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!head_block_) {
        // Minimum allocation size of 2MB for huge page / TLB friendliness
        size_t alloc_size = std::max<size_t>(initial_capacity_bytes, 2 * 1024 * 1024);
        head_block_ = AllocateOSBlock(alloc_size);
    }
}

void IsolatedAllocator::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    Block* curr = head_block_;
    while (curr) {
        Block* next = curr->next;
        FreeOSBlock(curr);
        curr = next;
    }
    head_block_ = nullptr;
}

IsolatedAllocator::Block* IsolatedAllocator::AllocateOSBlock(size_t size) {
    // We add sizeof(Block) to the size to store block metadata inline at the start
    size_t total_size = size + sizeof(Block);
    void* mapped_mem = nullptr;

#ifdef _WIN32
    // VirtualAlloc on Windows
    mapped_mem = VirtualAlloc(NULL, total_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    // mmap on POSIX. Use MAP_ANONYMOUS | MAP_PRIVATE.
#if defined(MAP_ANON) && !defined(MAP_ANONYMOUS)
#define MAP_ANONYMOUS MAP_ANON
#endif
    mapped_mem = mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped_mem == MAP_FAILED) {
        mapped_mem = nullptr;
    }
#endif

    if (!mapped_mem) {
        throw std::bad_alloc();
    }

    Block* block = static_cast<Block*>(mapped_mem);
    block->memory = static_cast<char*>(mapped_mem) + sizeof(Block);
    block->size = size;
    block->used = 0;
    block->next = nullptr;
    return block;
}

void IsolatedAllocator::FreeOSBlock(Block* block) {
    if (!block) return;
    
    size_t total_size = block->size + sizeof(Block);
#ifdef _WIN32
    VirtualFree(block, 0, MEM_RELEASE);
#else
    munmap(block, total_size);
#endif
}

void* IsolatedAllocator::Allocate(size_t size, size_t alignment) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!head_block_) {
        // Auto-initialize if not done explicitly, fallback 2MB chunk
        head_block_ = AllocateOSBlock(2 * 1024 * 1024);
    }

    Block* curr = head_block_;
    Block* last = nullptr;

    while (curr) {
        uintptr_t base_addr = reinterpret_cast<uintptr_t>(curr->memory) + curr->used;
        uintptr_t aligned_addr = (base_addr + alignment - 1) & ~(alignment - 1);
        size_t padding = aligned_addr - base_addr;

        if (curr->used + padding + size <= curr->size) {
            curr->used += padding + size;
            return reinterpret_cast<void*>(aligned_addr);
        }
        last = curr;
        curr = curr->next;
    }

    // Allocate a new block
    size_t new_block_size = std::max<size_t>(size * 2, 2 * 1024 * 1024);
    Block* new_block = AllocateOSBlock(new_block_size);
    if (last) {
        last->next = new_block;
    } else {
        head_block_ = new_block;
    }

    uintptr_t base_addr = reinterpret_cast<uintptr_t>(new_block->memory);
    uintptr_t aligned_addr = (base_addr + alignment - 1) & ~(alignment - 1);
    size_t padding = aligned_addr - base_addr;
    new_block->used += padding + size;

    return reinterpret_cast<void*>(aligned_addr);
}

void IsolatedAllocator::Deallocate(void* /*ptr*/, size_t /*size*/) {
    // For this simple bump allocator, we do not implement individual chunk deallocation.
    // The memory is reclaimed entirely when Shutdown() is called.
    // In a fully featured allocator, we would add the chunk to a free list.
    // Given the framework's use case (mostly tracking persistent state until shutdown),
    // this is acceptable for V1.
}

} // namespace memory
} // namespace domovoy
