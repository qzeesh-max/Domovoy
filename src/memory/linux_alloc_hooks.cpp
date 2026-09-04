#if defined(__linux__)

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#include <cstdlib>
#include <malloc.h>
#include <mutex>
#include <atomic>
#include "parallel_hashmap/phmap.h"
#include "domovoy/memory.h"
#include "domovoy/core.h"
#include "domovoy/allocator.h"

// Types for original libc functions
typedef void* (*malloc_func_t)(size_t);
typedef void  (*free_func_t)(void*);
typedef void* (*calloc_func_t)(size_t, size_t);
typedef void* (*realloc_func_t)(void*, size_t);
typedef void* (*memalign_func_t)(size_t, size_t);
typedef int   (*posix_memalign_func_t)(void**, size_t, size_t);

static malloc_func_t real_malloc = nullptr;
static free_func_t real_free = nullptr;
static calloc_func_t real_calloc = nullptr;
static realloc_func_t real_realloc = nullptr;
static memalign_func_t real_memalign = nullptr;
static posix_memalign_func_t real_posix_memalign = nullptr;

// Bootstrap buffer for dlsym which calls calloc
static char bootstrap_buf[65536];
static size_t bootstrap_idx = 0;
// Note: thread_local might allocate memory on first use, so we use a simple global since InitHooks is only called once.
static bool in_dlsym = false;

static std::atomic<bool> hooks_initialized{false};
static std::atomic<bool> hooks_initializing{false};

// We use phmap::parallel_flat_hash_map which partitions the map into 16 submaps,
// each with its own spinlock. It's extremely fast and cache-friendly.
using AllocMapType = phmap::parallel_flat_hash_map<
    const void*, 
    size_t,
    phmap::priv::hash_default_hash<const void*>,
    phmap::priv::hash_default_eq<const void*>,
    domovoy::memory::StlAllocator<std::pair<const void* const, size_t>>,
    4, // submaps = 2^4 = 16
    std::mutex
>;

static AllocMapType* g_allocations = nullptr;

static void InitHooks() {
    in_dlsym = true;
    real_malloc = (malloc_func_t)dlsym(RTLD_NEXT, "malloc");
    real_free = (free_func_t)dlsym(RTLD_NEXT, "free");
    real_calloc = (calloc_func_t)dlsym(RTLD_NEXT, "calloc");
    real_realloc = (realloc_func_t)dlsym(RTLD_NEXT, "realloc");
    real_memalign = (memalign_func_t)dlsym(RTLD_NEXT, "memalign");
    real_posix_memalign = (posix_memalign_func_t)dlsym(RTLD_NEXT, "posix_memalign");
    in_dlsym = false;

    // Allocate the map itself using the isolated allocator to prevent recursive loops
    void* map_memory = domovoy::memory::IsolatedAllocator::GetInstance().Allocate(sizeof(AllocMapType), alignof(AllocMapType));
    g_allocations = new (map_memory) AllocMapType();
}

static inline void EnsureInit() {
    if (hooks_initialized.load(std::memory_order_acquire)) return;
    if (hooks_initializing.exchange(true, std::memory_order_acquire)) {
        while (!hooks_initialized.load(std::memory_order_acquire)) {}
        return;
    }
    InitHooks();
    hooks_initialized.store(true, std::memory_order_release);
}

static inline void AddAllocation(void* ptr, size_t size) {
    if (!ptr || !domovoy::DomovoyCore::GetConfig().enable_leak_detection) return;
    if (!g_allocations) return; // Happens during bootstrap
    g_allocations->insert_or_assign(ptr, size);
}

static inline void RemoveAllocation(void* ptr) {
    if (!ptr || !domovoy::DomovoyCore::GetConfig().enable_leak_detection) return;
    if (!g_allocations) return;
    g_allocations->erase(ptr);
}

extern "C" {

void* malloc(size_t size) {
    if (!real_malloc) {
        if (in_dlsym) {
            if (bootstrap_idx + size <= sizeof(bootstrap_buf)) {
                void* p = bootstrap_buf + bootstrap_idx;
                bootstrap_idx += size;
                return p;
            }
            return nullptr;
        }
        EnsureInit();
    }
    void* ptr = real_malloc(size);
    AddAllocation(ptr, size);
    return ptr;
}

void free(void* ptr) {
    if (!real_free) {
        if (ptr >= bootstrap_buf && ptr < bootstrap_buf + sizeof(bootstrap_buf)) {
            return;
        }
        EnsureInit();
    }
    RemoveAllocation(ptr);
    real_free(ptr);
}

void* calloc(size_t nmemb, size_t size) {
    if (!real_calloc) {
        if (in_dlsym) {
            size_t total = nmemb * size;
            if (bootstrap_idx + total <= sizeof(bootstrap_buf)) {
                void* p = bootstrap_buf + bootstrap_idx;
                bootstrap_idx += total;
                for (size_t i = 0; i < total; ++i) {
                    ((char*)p)[i] = 0;
                }
                return p;
            }
            return nullptr;
        }
        EnsureInit();
    }
    void* ptr = real_calloc(nmemb, size);
    AddAllocation(ptr, nmemb * size);
    return ptr;
}

void* realloc(void* ptr, size_t size) {
    if (!real_realloc) {
        EnsureInit();
    }
    void* new_ptr = real_realloc(ptr, size);
    if (ptr) RemoveAllocation(ptr);
    AddAllocation(new_ptr, size);
    return new_ptr;
}

void* memalign(size_t alignment, size_t size) {
    if (!real_memalign) {
        EnsureInit();
    }
    void* ptr = real_memalign(alignment, size);
    AddAllocation(ptr, size);
    return ptr;
}

int posix_memalign(void** memptr, size_t alignment, size_t size) {
    if (!real_posix_memalign) {
        EnsureInit();
    }
    int res = real_posix_memalign(memptr, alignment, size);
    if (res == 0 && memptr && *memptr) {
        AddAllocation(*memptr, size);
    }
    return res;
}

} // extern "C"

namespace domovoy {
namespace memory {
    void GetLinuxTrackedAllocations(AllocationList& out_list) {
        if (!g_allocations) return;
        
        // parallel_hashmap allows safe iteration if we don't modify it.
        // During shutdown, mutations are mostly stopped, but we lock all submaps by copying via iterators
        // or just rely on the fact that during leak reporting, the process is shutting down.
        for (const auto& kv : *g_allocations) {
            out_list.push_back({ (void*)kv.first, kv.second, false });
        }
    }
}
}

// Dummy function to force linker to include this object file
extern "C" void domovoy_linux_alloc_hooks_init() {
    EnsureInit();
}

#endif // __linux__
