#include <gtest/gtest.h>
#include "domovoy/core.h"
#include <fcntl.h>
#include <unistd.h>

TEST(IntegrationTest, IoLeak) {
    domovoy::DomovoyConfig config;
    config.output_dir = ".";
    // Enable IO monitoring in our test config
    // Actually, IO monitoring is enabled by default in DomovoyConfig
    domovoy::DomovoyCore::Init(config);

    // Intentionally leak a file descriptor
    int fd = open("test_io_leak.tmp", O_CREAT | O_RDWR, 0666);
    ASSERT_GE(fd, 0);

    // Shutdown should report the leaked FD
    domovoy::DomovoyCore::Shutdown();

    // Clean up the file
    close(fd);
    unlink("test_io_leak.tmp");

    SUCCEED();
}
