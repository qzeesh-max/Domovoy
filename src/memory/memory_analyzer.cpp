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

#include "domovoy/memory.h"
#include "domovoy/core.h"
#include "domovoy/reporter.h"
#include <iostream>
#include <algorithm>
#include <cstdio>
#include <cctype>

#include <cpptrace/cpptrace.hpp>

#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>
#endif

#if defined(__APPLE__)
#include <malloc/malloc.h>
#include <mach/mach.h>
#elif defined(_WIN32)
#include <windows.h>
#include <typeinfo>

#ifdef _WIN64
struct RTTICompleteObjectLocator {
    unsigned long signature;
    unsigned long offset;
    unsigned long cdOffset;
    unsigned long pTypeDescriptor;
    unsigned long pClassDescriptor;
    unsigned long pSelf;
};
#else
struct RTTICompleteObjectLocator {
    unsigned long signature;
    unsigned long offset;
    unsigned long cdOffset;
    struct TypeDescriptor* pTypeDescriptor;
    void* pClassDescriptor;
};
#endif
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

    // Filter and sort leaked allocations
    AllocationList leaked_allocations;
    size_t total_leaks = 0;
    size_t total_leaked_bytes = 0;

    for (const auto& alloc : allocations) {
        if (!alloc.marked) {
            leaked_allocations.push_back(alloc);
            total_leaks++;
            total_leaked_bytes += alloc.size;
        }
    }

    std::sort(leaked_allocations.begin(), leaked_allocations.end(), [](const AllocationRecord& a, const AllocationRecord& b) {
        if (a.size != b.size) return a.size > b.size;
        return a.address < b.address;
    });

    auto resolve_type = [](void* address, size_t size) -> isolated_string {
        if (size < sizeof(void*)) return "";
        void* vptr = *(void**)address;

#if defined(_WIN32)
        HMODULE hModule = NULL;
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                (LPCSTR)vptr, &hModule)) {
            return ""; 
        }
#endif
        
#if defined(_MSC_VER)
        const char* type_name_str = "";
        __try {
            void* rtti_ptr = ((void**)vptr)[-1];
            if (rtti_ptr) {
                RTTICompleteObjectLocator* pLocator = (RTTICompleteObjectLocator*)rtti_ptr;
#ifdef _WIN64
                if (pLocator->signature == 1) {
                    void* baseAddress = nullptr;
                    RtlPcToFileHeader((PVOID)pLocator, &baseAddress);
                    if (baseAddress) {
                        std::type_info* pType = (std::type_info*)((char*)baseAddress + pLocator->pTypeDescriptor);
                        type_name_str = pType->name();
                    }
                }
#else
                if (pLocator->signature == 0) {
                    std::type_info* pType = (std::type_info*)pLocator->pTypeDescriptor;
                    type_name_str = pType->name();
                }
#endif
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            // Access violation reading RTTI, not a valid polymorphic object
        }
        
        if (type_name_str && type_name_str[0] != '\0') {
            std::string class_name = type_name_str;
            // MSVC type_info::name() returns strings like "class MyClass" or "struct MyStruct"
            if (class_name.find("class ") == 0) {
                class_name = class_name.substr(6);
            } else if (class_name.find("struct ") == 0) {
                class_name = class_name.substr(7);
            }
            return isolated_string(class_name.c_str());
        }
        
#elif defined(__GNUC__) && defined(_WIN32)
        // MinGW/GCC on Windows (Itanium ABI). Safe RTTI extraction avoiding virtual calls.
        void* rtti_ptr_loc = (void*)(((void**)vptr) - 1);
        if (!IsBadReadPtr(rtti_ptr_loc, sizeof(void*))) {
            void* pType = ((void**)vptr)[-1];
            if (pType && !IsBadReadPtr(pType, sizeof(void*) * 2)) {
                // Itanium type_info layout: vptr at [0], __name at [1]
                const char* type_name_str = ((const char**)pType)[1];
                if (type_name_str && !IsBadReadPtr((void*)type_name_str, 1)) {
                    // Check if string looks like a valid mangled name (basic heuristic to avoid random garbage)
                    if (isalnum((unsigned char)type_name_str[0]) || type_name_str[0] == 'N' || type_name_str[0] == 'Z') {
                        int status = -1;
                        char* demangled = abi::__cxa_demangle(type_name_str, nullptr, nullptr, &status);
                        if (status == 0 && demangled) {
                            isolated_string result(demangled);
                            free(demangled);
                            return result;
                        }
                    }
                }
            }
        }
#endif

        cpptrace::raw_trace trace;
        trace.frames.push_back(reinterpret_cast<uintptr_t>(vptr));
        auto resolved = trace.resolve();
        
        if (!resolved.frames.empty()) {
            std::string sym = resolved.frames[0].symbol;
            if (!sym.empty()) {
                if (sym.find("vtable for ") == 0) {
                    return isolated_string(sym.substr(11).c_str());
                }
                return isolated_string(sym.c_str());
            }
        }
        return "";
    };

    isolated_json<> report;
    report["type"] = "memory_leaks";
    report["summary"] = isolated_json<>::object();
    report["summary"]["total_leaks"] = total_leaks;
    report["summary"]["total_leaked_bytes"] = total_leaked_bytes;
    
    auto& summary_by_size = report["summary"]["by_size"] = isolated_json<>::array();
    auto& leaks_by_size = report["leaks_by_size"] = isolated_json<>::array();

    size_t current_size = 0;
    size_t current_count = 0;
    isolated_json<> current_sample_addresses = isolated_json<>::array();
    isolated_json<> current_leak_group;

    auto flush_group = [&]() {
        if (current_count > 0) {
            isolated_json<> summary_item;
            summary_item["size"] = current_size;
            summary_item["count"] = current_count;
            summary_item["sample_addresses"] = current_sample_addresses;
            summary_by_size.push_back(summary_item);
            
            leaks_by_size.push_back(current_leak_group);
        }
    };

    for (const auto& alloc : leaked_allocations) {
        if (alloc.size != current_size) {
            flush_group();
            current_size = alloc.size;
            current_count = 0;
            current_sample_addresses = isolated_json<>::array();
            current_leak_group = isolated_json<>::object();
            current_leak_group["size"] = current_size;
            current_leak_group["addresses"] = isolated_json<>::array();
        }
        
        current_count++;
        
        char addr_buf[32];
        snprintf(addr_buf, sizeof(addr_buf), "0x%zx", reinterpret_cast<uintptr_t>(alloc.address));
        isolated_string addr_str(addr_buf);

        if (current_sample_addresses.size() < 100) {
            current_sample_addresses.push_back(addr_str);
        }
        
        isolated_json<> leak_detail;
        leak_detail["address"] = addr_str;
        isolated_string type_name = resolve_type(alloc.address, alloc.size);
        if (!type_name.empty()) {
            leak_detail["type"] = type_name;
        }
        
        current_leak_group["addresses"].push_back(leak_detail);
    }
    flush_group();

    if (total_leaks > 0) {
        Reporter* reporter = DomovoyCore::GetReporter();
        if (reporter) {
            reporter->ReportLeaks(report);
        }
    }
}

} // namespace memory
} // namespace domovoy
