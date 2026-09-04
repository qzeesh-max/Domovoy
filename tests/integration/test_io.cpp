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
#include "test_utils.h"

#if defined(_WIN32)
#include <windows.h>
#endif

TEST(IntegrationTest, IoLeak) {
    CleanUpOldReports();
    
    domovoy::DomovoyConfig config;
    config.output_dir = ".";
    // Enable IO monitoring in our test config
    // Actually, IO monitoring is enabled by default in DomovoyConfig
    domovoy::DomovoyCore::Init(config);

    // Intentionally leak a file descriptor
#if defined(_WIN32)
    HANDLE fd = CreateFileA("test_io_leak.tmp", GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    ASSERT_NE(fd, INVALID_HANDLE_VALUE);
#else
    int fd = open("test_io_leak.tmp", O_CREAT | O_RDWR, 0666);
    ASSERT_GE(fd, 0);
#endif

    // Shutdown should report the leaked FD
    domovoy::DomovoyCore::Shutdown();

    nlohmann::json report = FindAndParseReport("domovoy_io_leak");
    ASSERT_FALSE(report.is_null()) << "IO Leak report was not generated!";
    ASSERT_EQ(report["type"], "fd_leak");

    // Clean up the file
#if defined(_WIN32)
    CloseHandle(fd);
#else
    close(fd);
#endif
    unlink("test_io_leak.tmp");

    SUCCEED();
}
