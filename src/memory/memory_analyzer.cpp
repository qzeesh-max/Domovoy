#include "domovoy/memory.h"
#include "domovoy/core.h"
#include "domovoy/reporter.h"
#include <iostream>

#if defined(__APPLE__)
#include <malloc/malloc.h>
#include <mach/mach.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace domovoy {
namespace memory {

MemoryAnalyzer& MemoryAnalyzer::GetInstance() {
    static MemoryAnalyzer instance;
    return instance;
}

#if defined(__APPLE__)
// Helper struct to pass context to the block enumerator
struct EnumContext {
    AllocationList* allocations;
};

static void BlockEnumerator(task_t /*task*/, void* context, unsigned /*type*/, vm_range_t* ranges, unsigned count) {
    auto* ctx = static_cast<EnumContext*>(context);
    for (unsigned i = 0; i < count; ++i) {
        ctx->allocations->push_back({
            reinterpret_cast<void*>(ranges[i].address),
            ranges[i].size,
            false // not marked yet
        });
    }
}
#endif

void MemoryAnalyzer::EnumerateHeap(AllocationList& allocations) {
#if defined(__APPLE__)
    vm_address_t* zones = nullptr;
    unsigned count = 0;
    
    // In a real implementation, you'd use mach_task_self() and vm_read, but this is a simplified version
    // using the local process zone enumeration.
    malloc_zone_t** zone_array = nullptr;
    unsigned zone_count = 0;
    kern_return_t kr = malloc_get_all_zones(mach_task_self(), nullptr, (vm_address_t**)&zone_array, &zone_count);
    
    if (kr == KERN_SUCCESS && zone_array) {
        EnumContext ctx{&allocations};
        for (unsigned i = 0; i < zone_count; ++i) {
            malloc_zone_t* zone = zone_array[i];
            if (zone && zone->introspect && zone->introspect->enumerator) {
                // Warning: enumerator can deadlock if threads are currently allocating in this zone.
                // A robust BDW GC suspends all threads before doing this. 
                // We assume this is called on shutdown or during a fatal crash where we only have one active thread.
                zone->introspect->enumerator(mach_task_self(), &ctx, MALLOC_PTR_IN_USE_RANGE_TYPE, (vm_address_t)zone, nullptr, BlockEnumerator);
            }
        }
    }
#elif defined(_WIN32)
    DWORD numHeaps = GetProcessHeaps(0, NULL);
    if (numHeaps > 0) {
        std::vector<HANDLE> heaps(numHeaps);
        GetProcessHeaps(numHeaps, heaps.data());
        for (HANDLE heap : heaps) {
            PROCESS_HEAP_ENTRY entry;
            entry.lpData = NULL;
            // HeapWalk can be extremely slow and requires the heap to be locked.
            // Best effort scan.
            HeapLock(heap);
            while (HeapWalk(heap, &entry) != FALSE) {
                if (entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) {
                    allocations.push_back({entry.lpData, entry.cbData, false});
                }
            }
            HeapUnlock(heap);
        }
    }
#else
    // Stub for Linux. Reliable enumeration requires intercepting malloc.
#endif
}

void MemoryAnalyzer::MarkReachability(AllocationList& allocations, void* ptr_value) {
    // Binary search could be used if allocations were sorted.
    // We'll do a simple linear scan or just mark logic.
    // To make it robust, we just check if ptr_value points *into* any block.
    // For large lists, this O(N) check per pointer is slow. 
    // In a real BDW GC, allocations are kept in an interval tree or pagemap.
    
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr_value);
    for (auto& alloc : allocations) {
        uintptr_t base = reinterpret_cast<uintptr_t>(alloc.address);
        if (addr >= base && addr < (base + alloc.size)) {
            alloc.marked = true;
            return;
        }
    }
}

void MemoryAnalyzer::ScanDataSegments(AllocationList& /*allocations*/) {
    // Stub: To scan data/BSS segments, we need to parse mach-o / PE / ELF headers
    // of the loaded modules.
}

void MemoryAnalyzer::ScanThreadStacks(AllocationList& /*allocations*/) {
    // Stub: Enumerate all threads, get their context/registers and stack bounds.
    // For the current thread:
    // void* dummy = nullptr;
    // scan from &dummy to pthread_get_stackaddr_np()
}

void MemoryAnalyzer::RunLeakDetection() {
    AllocationList allocations;
    EnumerateHeap(allocations);
    
    ScanDataSegments(allocations);
    ScanThreadStacks(allocations);

    // Reporting
    isolated_json<> report;
    report["type"] = "memory_leaks";
    auto& leaks = report["leaks"] = isolated_json<>::array();

    size_t total_leaks = 0;
    size_t total_leaked_bytes = 0;

    for (const auto& alloc : allocations) {
        if (!alloc.marked) {
            total_leaks++;
            total_leaked_bytes += alloc.size;
            
            isolated_json<> leak_info;
            leak_info["address"] = reinterpret_cast<uintptr_t>(alloc.address);
            leak_info["size"] = alloc.size;
            leaks.push_back(leak_info);
        }
    }

    report["summary"]["total_leaks"] = total_leaks;
    report["summary"]["total_leaked_bytes"] = total_leaked_bytes;

    if (total_leaks > 0) {
        Reporter* reporter = DomovoyCore::GetReporter();
        if (reporter) {
            reporter->ReportLeaks(report);
        }
    }
}

} // namespace memory
} // namespace domovoy
