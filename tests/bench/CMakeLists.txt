add_executable(bench_core bench_core.cpp)
target_compile_options(bench_core PRIVATE ${DOMOVOY_WARNING_FLAGS})
target_link_libraries(bench_core PRIVATE domovoy benchmark::benchmark benchmark::benchmark_main)
