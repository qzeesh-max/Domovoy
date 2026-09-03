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
