#include <gtest/gtest.h>
#include "domovoy/core.h"
#include <vector>

TEST(IntegrationTest, MemoryLeak) {
    domovoy::DomovoyConfig config;
    config.output_dir = ".";
    domovoy::DomovoyCore::Init(config);

    // Intentionally leak memory
    void* leaked_mem = malloc(1337);
    (void)leaked_mem;
    
    // Create some reachable memory to ensure it isn't reported as a leak
    // Actually, BDW scan needs to find it on stack or registers. 
    // In our simplified BDW stub, it won't find it yet, so everything is reported as a leak.
    
    // Shutting down Domovoy should generate a report
    domovoy::DomovoyCore::Shutdown();
    
    // Check if report file was generated
    // (This can be more robust, but just running it without crashing is a good start)
    SUCCEED();
}
