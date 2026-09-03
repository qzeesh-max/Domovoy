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
