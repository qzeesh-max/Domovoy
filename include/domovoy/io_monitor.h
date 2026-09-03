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

#pragma once

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include "domovoy/allocator.h"

namespace domovoy {
namespace io {

class IoMonitor {
public:
    static IoMonitor& GetInstance();

    void Enable();
    void Disable();
    bool IsEnabled() const;

    // Callbacks from hooks
    void OnOpen(int fd, const char* path);
    void OnClose(int fd);
    void OnSocket(int fd, int domain, int type, int protocol);

    void ReportLeakedFds();

private:
    IoMonitor() = default;
    ~IoMonitor() = default;

    bool enabled_{false};
    std::mutex mutex_;

    struct FdInfo {
        char path[1024];
        bool is_socket = false;
        // Ideally we'd store a stack trace here. cpptrace::generate_trace() might be too 
        // slow to run on every open(). For this prototype, we'll store basic info.
    };

    using FdMap = std::unordered_map<
        int, 
        FdInfo, 
        std::hash<int>, 
        std::equal_to<int>, 
        memory::StlAllocator<std::pair<const int, FdInfo>>
    >;
    
    FdMap open_fds_;
};

} // namespace io
} // namespace domovoy
