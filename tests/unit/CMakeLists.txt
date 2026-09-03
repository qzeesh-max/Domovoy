add_executable(test_allocator test_allocator.cpp)
target_compile_options(test_allocator PRIVATE ${DOMOVOY_WARNING_FLAGS})
target_link_libraries(test_allocator PRIVATE domovoy GTest::gtest GTest::gtest_main)
add_test(NAME AllocatorTest COMMAND test_allocator)
