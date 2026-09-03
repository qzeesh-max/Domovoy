#include <gtest/gtest.h>
#include "domovoy/core.h"

// Disable ASAN/UBSAN on this specific function so it actually crashes with SIGSEGV
// instead of getting caught by sanitizers (if they were enabled for the test target)
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
__attribute__((no_sanitize("address")))
#endif
#endif
void CauseCrash() {
    volatile int* p = nullptr;
    *p = 42; // Causes SIGSEGV
}

TEST(IntegrationTest, CrashHandler) {
    // In a real framework, catching a crash means the process exits after reporting.
    // For testing, we would usually run this in a separate process or EXPECT_DEATH.
    // We'll use Google Test's EXPECT_DEATH which forks on POSIX.
    
    EXPECT_DEATH({
        domovoy::DomovoyConfig config;
        config.output_dir = ".";
        domovoy::DomovoyCore::Init(config);
        
        CauseCrash();
        
        domovoy::DomovoyCore::Shutdown();
    }, "");
}
