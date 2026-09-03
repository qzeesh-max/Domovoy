cmake_minimum_required(VERSION 3.20)
project(Domovoy VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler Warnings will be applied per-target to avoid breaking dependencies
set(DOMOVOY_WARNING_FLAGS "")
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    list(APPEND DOMOVOY_WARNING_FLAGS -Wall -Wextra -Wpedantic)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
        list(APPEND DOMOVOY_WARNING_FLAGS -Wno-c99-extensions)
    endif()
elseif(MSVC)
    list(APPEND DOMOVOY_WARNING_FLAGS /W4 /WX)
endif()

# Threading
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

# Dependencies
include(FetchContent)

# Google Test
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Google Benchmark
FetchContent_Declare(
  googlebenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG        v1.8.3
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)
if(TARGET benchmark)
    target_compile_options(benchmark PRIVATE "-Wno-error=switch")
endif()

# nlohmann_json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

if(WIN32)
    FetchContent_Declare(
        detours
        GIT_REPOSITORY https://github.com/microsoft/Detours.git
        GIT_TAG main
    )
    FetchContent_GetProperties(detours)
    if(NOT detours_POPULATED)
        FetchContent_Populate(detours)
    endif()
    add_library(detours STATIC
        ${detours_SOURCE_DIR}/src/detours.cpp
        ${detours_SOURCE_DIR}/src/disasm.cpp
        ${detours_SOURCE_DIR}/src/image.cpp
        ${detours_SOURCE_DIR}/src/modules.cpp
        ${detours_SOURCE_DIR}/src/creatwth.cpp
    )
    target_include_directories(detours PUBLIC ${detours_SOURCE_DIR}/src)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64")
        target_compile_definitions(detours PRIVATE DETOURS_X64=1)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i686|x86")
        target_compile_definitions(detours PRIVATE DETOURS_X86=1)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
        target_compile_definitions(detours PRIVATE DETOURS_ARM64=1)
    endif()
endif()

# cpptrace (for stack traces)
FetchContent_Declare(
    cpptrace
    GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
    GIT_TAG v0.7.3
)
# Disable ZSTD for cpptrace to avoid old zstd cmake issues
set(CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF_ZSTD OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cpptrace)

# Sanitizers
include(cmake/Sanitizers.cmake)

# Core Library
file(GLOB_RECURSE SRC_FILES "src/*.cpp")
add_library(domovoy STATIC ${SRC_FILES})
target_include_directories(domovoy PUBLIC include)
target_compile_options(domovoy PRIVATE ${DOMOVOY_WARNING_FLAGS})
target_link_libraries(domovoy PUBLIC Threads::Threads nlohmann_json::nlohmann_json cpptrace::cpptrace)
if(WIN32)
    target_link_libraries(domovoy PUBLIC detours)
endif()

if(APPLE)
    # Required for malloc_get_all_zones, etc. if needed later, but they are in libc anyway.
    # For Mach exceptions or thread state we might need specific frameworks, but let's stick to POSIX mostly.
endif()

# Tests
enable_testing()
add_subdirectory(tests/unit)
add_subdirectory(tests/integration)
add_subdirectory(tests/bench)
