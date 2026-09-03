#include <benchmark/benchmark.h>
#include "domovoy/core.h"
#include "domovoy/allocator.h"
#include <thread>
#include <vector>
#include <chrono>

static void BM_IsolatedAllocator(benchmark::State& state) {
    domovoy::DomovoyConfig config;
    domovoy::DomovoyCore::Init(config);

    for (auto _ : state) {
        void* ptr = domovoy::memory::IsolatedAllocator::GetInstance().Allocate(state.range(0));
        benchmark::DoNotOptimize(ptr);
        // We don't deallocate because it's a bump allocator in this prototype, 
        // but we'll simulate a fast path allocation.
        // In a real bench, we'd want to reset or use a thread-local pool.
    }
    
    domovoy::DomovoyCore::Shutdown();
}
// Benchmark allocations of 64 bytes to 4KB
BENCHMARK(BM_IsolatedAllocator)->Range(64, 4096);

static void BM_CpuProfilerOverhead(benchmark::State& state) {
    domovoy::DomovoyConfig config;
    config.enable_cpu_profiler = true;
    domovoy::DomovoyCore::Init(config);

    // Simulate some work on multiple threads
    std::vector<std::thread> threads;
    for (int i = 0; i < state.range(0); ++i) {
        threads.emplace_back([]() {
            auto start = std::chrono::steady_clock::now();
            int sink = 0;
            while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(10)) {
                int x = 0;
                for (int j = 0; j < 1000; ++j) x += j;
                sink += x;
            }
            benchmark::DoNotOptimize(sink);
        });
    }

    for (auto _ : state) {
        // Just measure the time it takes to run with the profiler on
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    for (auto& t : threads) {
        t.join();
    }
    domovoy::DomovoyCore::Shutdown();
}
BENCHMARK(BM_CpuProfilerOverhead)->Arg(1)->Arg(4)->Arg(8);

BENCHMARK_MAIN();
