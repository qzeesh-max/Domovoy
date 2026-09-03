# Domovoy

> A cross-platform, lightweight C++20 framework for **production observability** — gathering memory leaks, heap corruption, CPU hotspots, IO leaks, and crash context with minimal overhead.

Domovoy is designed to be embedded in shipped software so that you can gather high-quality diagnostics from production crashes and slow sessions, in a way that is **safe, isolated from application heap corruption, and fully opt-in**.

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Architecture](#architecture)
- [Requirements](#requirements)
- [Quick Start](#quick-start)
- [Build System](#build-system)
  - [Build Options](#build-options)
  - [Sanitizer Builds](#sanitizer-builds)
- [Test Scripts](#test-scripts)
  - [Unit Tests](#unit-tests)
  - [Integration Tests](#integration-tests)
  - [Benchmarks](#benchmarks)
  - [Full Suite](#full-suite)
- [Docker / Containerized Testing](#docker--containerized-testing)
  - [Building Images](#building-images)
  - [Running Tests in Docker](#running-tests-in-docker)
  - [Docker Compose](#docker-compose)
- [Usage in Your Application](#usage-in-your-application)
  - [Initialization](#initialization)
  - [Configuration Reference](#configuration-reference)
  - [Custom Reporters](#custom-reporters)
- [Output Format](#output-format)
  - [Memory Leak Report](#memory-leak-report)
  - [CPU Hotspot Report](#cpu-hotspot-report)
  - [IO Leak Report](#io-leak-report)
  - [Crash Report](#crash-report)
- [Platform Support](#platform-support)
  - [macOS](#macos)
  - [Linux](#linux)
  - [Windows](#windows)
- [Performance Characteristics](#performance-characteristics)
- [Subsystem Deep-Dives](#subsystem-deep-dives)
  - [IsolatedAllocator](#isolatedallocator)
  - [MemoryAnalyzer](#memoryanalyzer)
  - [CpuMonitor](#cpumonitor)
  - [IoMonitor](#iomonitor)
  - [CrashHandler](#crashhandler)
- [Project Structure](#project-structure)
- [Contributing](#contributing)
- [License](#license)

---

## Overview

Modern C++ applications deployed to end-users suffer from subtle resource problems that are nearly impossible to reproduce in development:

- **Memory leaks** that accumulate over hours of uptime
- **Heap corruption** that silently corrupts unrelated allocations
- **CPU regressions** visible only on real-world workloads
- **IO leaks** — file descriptors and sockets not closed on error paths
- **Silent crashes** — processes that die with no useful crash log

Domovoy hooks into the process lifecycle at `Init()` and `Shutdown()`, passively gathers telemetry, and writes a structured **JSON report** to a location of your choosing. It operates on a **separate OS-mapped memory region** so its own allocations are immune to heap corruption in your application.

---

## Features

| Feature | macOS | Linux | Windows |
|---|---|---|---|
| **Isolated Allocator** (OS-mapped memory) | ✅ | ✅ | ✅ |
| **Memory Leak Detection** (heap walk) | ✅ | ✅ (planned) | ✅ (planned) |
| **Heap Corruption Detection** | 🔜 | 🔜 | 🔜 |
| **CPU Hotspot Profiling** (passive thread) | ✅ | ✅ | ✅ |
| **IO Leak Detection** (`open`/`socket`/`close`) | ✅ `DYLD_INTERPOSE` | ✅ `LD_PRELOAD` | ✅ Detours |
| **Crash Context** (backtrace on fatal signal) | ✅ `sigaction` | ✅ `sigaction` | ✅ VEH |
| **JSON Reporting** | ✅ | ✅ | ✅ |
| **Custom Reporter Interface** | ✅ | ✅ | ✅ |

---

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│                    Your Application                       │
│                                                          │
│   DomovoyCore::Init(config)  ←  explicit, user-driven   │
│   DomovoyCore::Shutdown()    ←  explicit, user-driven   │
└────────────────────┬─────────────────────────────────────┘
                     │
          ┌──────────▼──────────┐
          │     DomovoyCore     │  Singleton lifecycle
          └──────────┬──────────┘
                     │
        ┌────────────┼─────────────────┐──────────────┐
        ▼            ▼                 ▼              ▼
  ┌───────────┐ ┌──────────┐ ┌──────────────┐ ┌──────────────┐
  │MemoryAnal-│ │CpuMonitor│ │  IoMonitor   │ │CrashHandler  │
  │yzer       │ │(bg thread)│ │(interposition│ │(alt-stack /  │
  │(heap walk)│ │           │ │ hooks)       │ │ VEH)         │
  └─────┬─────┘ └─────┬────┘ └──────┬───────┘ └──────┬───────┘
        │             │             │                 │
        └─────────────┴─────────────┴─────────────────┘
                                   │
                          ┌────────▼────────┐
                          │   Reporter       │  Interface
                          │  (JsonFileRep.)  │
                          └────────┬────────┘
                                   │
                          ┌────────▼────────┐
                          │  IsolatedAlloc. │  mmap / VirtualAlloc
                          │  (OS memory)    │  never touches app heap
                          └─────────────────┘
```

All internal data structures — strings, maps, JSON nodes — allocate through `IsolatedAllocator`, which uses raw OS pages separate from the application's heap. This ensures Domovoy remains functional even when the application's heap is corrupted.

---

## Requirements

| Dependency | Version | Notes |
|---|---|---|
| **CMake** | ≥ 3.20 | |
| **C++ compiler** | C++20 | GCC 11+, Clang 14+, AppleClang 14+, MSVC 19.30+ |
| **Git** | any | For `FetchContent` dependency fetching |
| **Python 3** | any | Required by some CMake detection scripts |
| **Docker** *(optional)* | 20.10+ | For containerized cross-platform testing |

All other dependencies (GoogleTest, Google Benchmark, nlohmann/json, cpptrace, libdwarf, Microsoft Detours on Windows) are automatically fetched by CMake `FetchContent` at configure time.

---

## Quick Start

```bash
# Clone
git clone https://github.com/yourorg/domovoy.git
cd domovoy

# Build (Debug)
./scripts/build.sh

# Run all tests
./scripts/test_all.sh

# Run benchmarks
./scripts/test_bench.sh
```

---

## Build System

Domovoy uses CMake as its build system. A convenience wrapper script `scripts/build.sh` is provided.

### Build Options

```bash
# Debug build (default)
./scripts/build.sh

# Release build
./scripts/build.sh --type Release

# RelWithDebInfo (release + debug symbols, useful for profiling)
./scripts/build.sh --type RelWithDebInfo

# Clean + rebuild
./scripts/build.sh --clean

# Parallel jobs
./scripts/build.sh --jobs 16
```

Alternatively, invoke CMake directly for full control:

```bash
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DENABLE_ASAN=ON

cmake --build build --parallel 8
```

### Sanitizer Builds

Domovoy includes `cmake/Sanitizers.cmake` which exposes the following CMake options:

| Flag | Description |
|---|---|
| `-DENABLE_ASAN=ON` | AddressSanitizer — detects heap/stack/global buffer overflows, use-after-free |
| `-DENABLE_TSAN=ON` | ThreadSanitizer — detects data races |
| `-DENABLE_UBSAN=ON` | UndefinedBehaviorSanitizer — detects signed overflow, null deref, etc. |
| `-DENABLE_MSAN=ON` | MemorySanitizer — detects reads of uninitialized memory (Clang only) |

> **Note:** TSan and ASan cannot be used together. MSAN requires a fully instrumented toolchain.

Via script:
```bash
# ASan + UBSan
./scripts/build.sh --asan --ubsan

# TSan
./scripts/build.sh --tsan
```

---

## Test Scripts

All test scripts live under `scripts/` and are self-contained. They will automatically invoke `build.sh` if the build directory does not exist or is stale.

### Unit Tests

Unit tests cover the `IsolatedAllocator` in isolation.

```bash
# Run all unit tests
./scripts/test_unit.sh

# With AddressSanitizer
./scripts/test_unit.sh --asan

# Filter to a specific test
./scripts/test_unit.sh --filter "AllocatorTest.MultipleAllocations"

# Verbose output
./scripts/test_unit.sh --verbose
```

### Integration Tests

Integration tests exercise the complete framework lifecycle with real leak/CPU/IO/crash scenarios.

```bash
# Run all integration tests
./scripts/test_integration.sh

# Run a specific suite
./scripts/test_integration.sh --test leak
./scripts/test_integration.sh --test cpu
./scripts/test_integration.sh --test io
./scripts/test_integration.sh --test crash

# With sanitizers
./scripts/test_integration.sh --asan --ubsan

# Verbose
./scripts/test_integration.sh --verbose
```

### Benchmarks

Benchmarks are built in **Release** mode to produce meaningful numbers.

```bash
# Run all benchmarks
./scripts/test_bench.sh

# Filter
./scripts/test_bench.sh --filter "BM_IsolatedAllocator"

# Output as JSON (for CI artifact storage)
./scripts/test_bench.sh --format json --out bench_results.json

# Longer minimum time for more stable results
./scripts/test_bench.sh --min-time 3.0
```

### Full Suite

```bash
# Run everything (unit + integration + benchmarks)
./scripts/test_all.sh

# With ASan for unit + integration, benchmarks skipped
./scripts/test_all.sh --asan --skip-bench

# Verbose
./scripts/test_all.sh --verbose
```

---

## Docker / Containerized Testing

Two Docker images are provided to enable testing on Linux from any host (including macOS):

| Image | Base | Compiler | Purpose |
|---|---|---|---|
| `domovoy:ubuntu` | Ubuntu 22.04 | GCC 11 | Standard Linux build and test |
| `domovoy:clang` | Ubuntu 22.04 | Clang 17 | Sanitizer-focused testing |

### Building Images

```bash
# Build both images
./scripts/docker_build.sh

# Build a specific image
./scripts/docker_build.sh --image ubuntu
./scripts/docker_build.sh --image clang

# Force rebuild (no cache)
./scripts/docker_build.sh --no-cache
```

### Running Tests in Docker

```bash
# Run the full test suite in the Ubuntu container
./scripts/docker_test.sh --image ubuntu --suite all

# Run integration tests in the Clang container with ASan + UBSan
./scripts/docker_test.sh --image clang --suite integration --asan --ubsan

# Run only benchmarks
./scripts/docker_test.sh --image ubuntu --suite bench

# Open an interactive shell for debugging
./scripts/docker_test.sh --image clang --interactive
```

> **Security note:** The container needs `--cap-add SYS_PTRACE` and `--security-opt seccomp=unconfined` for sanitizers and the crash handler's `sigaltstack` to work correctly. The `docker_test.sh` script adds these automatically.

### Docker Compose

`docker/docker-compose.yml` orchestrates parallel runs across environments:

```bash
# Run Ubuntu tests and Clang sanitizer tests in parallel
docker compose -f docker/docker-compose.yml up

# Run only benchmarks (uses a Docker volume for results)
docker compose -f docker/docker-compose.yml --profile bench up benchmarks

# View benchmark results
docker compose -f docker/docker-compose.yml run \
    -v bench-results:/results ubuntu-tests \
    cat /results/bench_results.json
```

---

## Usage in Your Application

### Installation

Add Domovoy as a submodule or `FetchContent` dependency:

```cmake
include(FetchContent)
FetchContent_Declare(
    domovoy
    GIT_REPOSITORY https://github.com/yourorg/domovoy.git
    GIT_TAG        v1.0.0
)
FetchContent_MakeAvailable(domovoy)

target_link_libraries(your_app PRIVATE domovoy)
```

### Initialization

```cpp
#include <domovoy/core.h>

int main(int argc, char** argv) {
    // Configure Domovoy before your application starts
    domovoy::DomovoyConfig config;

    // Directory where JSON reports will be written
    config.output_dir = "/var/log/myapp/telemetry";

    // Feature flags
    config.enable_leak_detection          = true;
    config.enable_heap_corruption_detection = false; // Not yet implemented
    config.enable_cpu_profiler            = true;
    config.enable_io_monitoring           = true;
    config.enable_crash_handler           = true;

    // Memory for Domovoy's own internal allocator (8 MB default)
    config.allocator_capacity_bytes = 8 * 1024 * 1024;

    domovoy::DomovoyCore::Init(config);

    // ── Run your application ──────────────────────────────────────
    int result = run_app(argc, argv);
    // ─────────────────────────────────────────────────────────────

    // Flush reports and tear down Domovoy
    domovoy::DomovoyCore::Shutdown();
    return result;
}
```

### Configuration Reference

```cpp
struct DomovoyConfig {
    // Output directory for JSON reports (must be writable)
    const char* output_dir = ".";

    // Enable BDW-style heap walk to find unreachable allocations
    bool enable_leak_detection = true;

    // Enable heap redzone / canary checking (future)
    bool enable_heap_corruption_detection = false;

    // Enable passive background CPU profiling thread
    bool enable_cpu_profiler = true;

    // Enable IO (FD / socket) leak monitoring
    bool enable_io_monitoring = true;

    // Enable signal/VEH crash handler
    bool enable_crash_handler = true;

    // Size of Domovoy's private OS-mapped memory pool
    size_t allocator_capacity_bytes = 8 * 1024 * 1024; // 8 MB
};
```

### Custom Reporters

Implement the `Reporter` interface to send data to your own telemetry backend (e.g., a cloud metrics service):

```cpp
#include <domovoy/reporter.h>

class MyCloudReporter : public domovoy::Reporter {
public:
    void ReportLeaks(const domovoy::isolated_json<>& report) override {
        send_to_cloud("leaks", report.dump());
    }
    void ReportCpuHotspots(const domovoy::isolated_json<>& report) override {
        send_to_cloud("cpu", report.dump());
    }
    void ReportIoLeak(const domovoy::isolated_json<>& report) override {
        send_to_cloud("io_leaks", report.dump());
    }
    void ReportCrash(const domovoy::isolated_json<>& report) override {
        send_to_cloud("crash", report.dump());
    }
};
```

> **Note:** Custom reporter memory must be managed externally or allocated through `IsolatedAllocator` to maintain heap-corruption resilience.

---

## Output Format

All reports are written as JSON files to `config.output_dir`. Filenames include a subsystem prefix and a timestamp:

```
domovoy_leaks_1693600000000.json
domovoy_cpu_1693600000000.json
domovoy_io_leaks_1693600000000.json
domovoy_crash_1693600000000.json
```

### Memory Leak Report

```json
{
  "leaks": [
    { "address": 140234561024, "size": 1024 },
    { "address": 140234562048, "size": 4096 }
  ]
}
```

> The `address` field is the raw heap pointer of the unreachable allocation. Future versions will include allocation stack traces.

### CPU Hotspot Report

```json
{
  "cpu_hotspots": [
    {
      "thread_id": 12345,
      "cpu_time_ns": 15000000,
      "backtrace": [
        { "symbol": "ComputeMatrix()", "filename": "matrix.cpp", "line": 87 },
        { "symbol": "WorkerThread::Run()", "filename": "worker.cpp", "line": 42 }
      ]
    }
  ]
}
```

### IO Leak Report

```json
{
  "io_leaks": [
    { "fd": 4,  "path": "/etc/config.txt", "is_socket": false },
    { "fd": 7,  "path": "socket",           "is_socket": true  }
  ]
}
```

### Crash Report

```json
{
  "type": "crash",
  "signal": 11,
  "backtrace": [
    { "symbol": "CauseCrash()", "filename": "test_crash.cpp", "line": 12 },
    { "symbol": "main",         "filename": "test_crash.cpp", "line": 25 }
  ]
}
```

`signal` is the POSIX signal number (e.g. 11 = `SIGSEGV`) on POSIX, or the Windows exception code on Windows (e.g. `0xC0000005` = `EXCEPTION_ACCESS_VIOLATION`).

---

## Platform Support

### macOS

- **IO interposition**: Uses Apple's `DYLD_INTERPOSE` macro to replace `open(2)`, `close(2)`, and `socket(2)` without requiring `LD_PRELOAD` or dynamic library injection.
- **Heap walk**: Uses `malloc_get_all_zones()` / `vm_range_recorder` from `<malloc/malloc.h>` to enumerate heap zones.
- **Crash handling**: `sigaltstack` + `sigaction` with `SA_ONSTACK | SA_SIGINFO` for `SIGSEGV`, `SIGABRT`, `SIGILL`, `SIGFPE`, `SIGBUS`.
- **Stack traces**: `cpptrace` with `execinfo.h` unwinding and `libdwarf` for DWARF symbol resolution.

### Linux

- **IO interposition**: The `DYLD_INTERPOSE` hooks compile out on Linux. Linux support uses `__attribute__((visibility("default")))` function wrapping or `LD_PRELOAD` injection (planned).
- **Heap walk**: Linux `malloc` does not expose a public enumeration API. Domovoy will use `mallinfo2()` for aggregate stats and iterate `/proc/self/maps` (planned).
- **Crash handling**: Identical `sigaltstack` + `sigaction` path as macOS.
- **Stack traces**: `cpptrace` with `libunwind` or `execinfo` and DWARF symbols.

### Windows

- **IO interposition**: Microsoft Detours library hooks `CreateFileA`, `CreateFileW`, and `CloseHandle`.
- **Heap walk**: `HeapWalk()` Win32 API for full heap enumeration (planned).
- **Crash handling**: `AddVectoredExceptionHandler` catches `EXCEPTION_ACCESS_VIOLATION`, `EXCEPTION_ILLEGAL_INSTRUCTION`, `EXCEPTION_INT_DIVIDE_BY_ZERO`, and `EXCEPTION_STACK_OVERFLOW`.
- **Stack traces**: `cpptrace` with `DbgHelp` for symbol resolution.

---

## Performance Characteristics

Results measured on Apple M1 (10-core), Debug build. Release numbers are typically 3–5× better:

```
--------------------------------------------------------------------
Benchmark                          Time             CPU   Iterations
--------------------------------------------------------------------
BM_IsolatedAllocator/64          583 ns          582 ns      5,199,320
BM_IsolatedAllocator/512         964 ns          963 ns      1,000,000
BM_IsolatedAllocator/4096      18314 ns        18236 ns        854,732
BM_CpuProfilerOverhead/1     1264352 ns        13541 ns         10,000
BM_CpuProfilerOverhead/4     1265662 ns        13472 ns         10,000
BM_CpuProfilerOverhead/8     1266445 ns        13571 ns         10,000
```

Key observations:

- **Isolated Allocator**: A 64-byte allocation costs ~580 ns (it's a bump allocator; no per-allocation syscall after initial `mmap`). A 4 KB allocation triggers a new OS block, hence the higher latency.
- **CPU Profiler**: Monitoring 1–8 threads concurrently costs only **~13 µs of CPU time** per wakeup interval — negligible compared to the 1 ms check interval.
- **IO hooks**: `open(2)` interposition adds a single mutex lock + `strlcpy`. On a lightly loaded system this is < 1 µs.

---

## Subsystem Deep-Dives

### IsolatedAllocator

`include/domovoy/allocator.h` | `src/allocator.cpp`

A bump allocator backed by contiguous OS pages (`mmap` on POSIX, `VirtualAlloc` on Windows). Memory is never returned to the OS until `Shutdown()`. This approach:

1. **Eliminates internal fragmentation** for the framework's fixed-size working set.
2. **Survives application heap corruption** — the allocator's metadata is in separate pages not addressable by normal `malloc`/`free`.
3. **Is thread-safe** via a single `std::mutex` protecting the bump pointer.

An STL-compatible adapter, `StlAllocator<T>`, routes all internal `std::unordered_map`, `std::string`, and JSON node allocations through the isolated pool.

### MemoryAnalyzer

`include/domovoy/memory.h` | `src/memory/memory_analyzer.cpp`

Implements a Boehm-Demers-Weiser-inspired reachability analysis:

1. **Enumerate all heap blocks** using platform-native APIs (`malloc_zone` on macOS, `HeapWalk` on Windows).
2. **Scan all root regions** — stack, BSS, data segment — for pointer-sized values.
3. Any heap block whose address does not appear anywhere in the root scan is reported as a **potential leak**.

Limitations: conservative (no type information), false positives possible for blocks reachable only through encoded/compressed pointers.

### CpuMonitor

`include/domovoy/profiler.h` | `src/profiler/cpu_monitor.cpp`

A background `std::thread` wakes up every 100 ms and:

1. Enumerates all threads in the current process (platform API).
2. Reads each thread's accumulated CPU time.
3. If any thread consumed > 90% CPU in the last interval, captures a backtrace with `cpptrace`.
4. Stores the (thread-id → backtrace) map in `IsolatedAllocator`-backed storage.
5. On `Shutdown()`, reports all recorded hotspots through the `Reporter` interface.

The thread runs at `SCHED_OTHER` / `THREAD_PRIORITY_LOWEST` to minimize competition with application threads.

### IoMonitor

`include/domovoy/io_monitor.h` | `src/io/io_monitor.cpp`

Intercepts file descriptor lifecycle calls:

| Platform | Mechanism | Intercepted calls |
|---|---|---|
| macOS | `DYLD_INTERPOSE` | `open`, `open64`, `close`, `socket` |
| Linux | Wrapper functions + `LD_PRELOAD` (planned) | `open`, `openat`, `close`, `socket` |
| Windows | Microsoft Detours | `CreateFileA`, `CreateFileW`, `CloseHandle` |

On `Shutdown()`, any FD still present in the tracking map (not yet `close()`d) is reported as an IO leak, along with its path and whether it is a socket.

### CrashHandler

`include/domovoy/crash_handler.h` | `src/crash_handler.cpp`

Sets up an **alternate signal stack** (`sigaltstack`) so the handler runs even if the main stack has overflowed. Installs `SA_SIGINFO | SA_ONSTACK` handlers for `SIGSEGV`, `SIGABRT`, `SIGILL`, `SIGFPE`, `SIGBUS`.

On Windows, `AddVectoredExceptionHandler` is installed at `ULONG_MAX` priority (runs before any application SEH handlers).

When a crash is caught:
1. A `cpptrace` backtrace is captured.
2. The backtrace is serialized to JSON using the `Reporter`.
3. The previous (application) signal handler is re-invoked or the default disposition is restored so the process terminates normally (allowing core dumps if configured).

> **Safety caveat**: `cpptrace::generate_trace()` internally allocates memory. If the application heap is severely corrupted, this may fail. Future versions will explore async-signal-safe unwinding via a pre-allocated buffer.

---

## Project Structure

```
domovoy/
├── CMakeLists.txt              # Root CMake build
├── cmake/
│   └── Sanitizers.cmake        # Sanitizer option helpers
├── docker/
│   ├── Dockerfile.ubuntu       # GCC / Ubuntu image
│   ├── Dockerfile.clang        # Clang 17 / sanitizer image
│   └── docker-compose.yml      # Parallel multi-env orchestration
├── include/
│   └── domovoy/
│       ├── allocator.h         # IsolatedAllocator + StlAllocator
│       ├── core.h              # DomovoyCore + DomovoyConfig
│       ├── crash_handler.h     # CrashHandler
│       ├── io_monitor.h        # IoMonitor
│       ├── memory.h            # MemoryAnalyzer
│       ├── profiler.h          # CpuMonitor
│       └── reporter.h          # Reporter interface
├── scripts/
│   ├── build.sh                # Main build script
│   ├── test_unit.sh            # Unit test runner
│   ├── test_integration.sh     # Integration test runner
│   ├── test_bench.sh           # Benchmark runner
│   ├── test_all.sh             # Full suite runner
│   ├── docker_build.sh         # Build Docker images
│   └── docker_test.sh          # Run tests in Docker
└── src/
│   ├── allocator.cpp           # IsolatedAllocator implementation
│   ├── core.cpp                # DomovoyCore lifecycle
│   ├── crash_handler.cpp       # Signal / VEH crash handling
│   ├── reporter.cpp            # JsonFileReporter
│   ├── io/
│   │   └── io_monitor.cpp      # IO interposition hooks
│   ├── memory/
│   │   └── memory_analyzer.cpp # Heap walk + reachability
│   └── profiler/
│       └── cpu_monitor.cpp     # Background CPU monitor
└── tests/
    ├── bench/
    │   ├── CMakeLists.txt
    │   └── bench_core.cpp      # Google Benchmark tests
    ├── integration/
    │   ├── CMakeLists.txt
    │   ├── test_cpu.cpp        # CPU profiler integration test
    │   ├── test_crash.cpp      # Crash handler integration test
    │   ├── test_io.cpp         # IO leak integration test
    │   └── test_leak.cpp       # Memory leak integration test
    └── unit/
        ├── CMakeLists.txt
        └── test_allocator.cpp  # IsolatedAllocator unit tests
```

---

## Contributing

1. Fork the repository.
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Run `./scripts/test_all.sh --ubsan` before committing.
4. Ensure new code passes `clang-tidy` (`.clang-tidy` coming soon).
5. Submit a pull request with a description of what the change does and why.

### Code Style

- C++20; no raw `new`/`delete` — use `IsolatedAllocator` or `std::unique_ptr`.
- All internal strings via `isolated_string`; all JSON via `isolated_json<>`.
- POSIX-guarded platform code with `#if defined(__APPLE__)`, `#elif defined(__linux__)`, `#elif defined(_WIN32)`.
- Every new subsystem should include at least one integration test.

---

## License

Domovoy is released under the **MIT License**. See [LICENSE](LICENSE) for details.
