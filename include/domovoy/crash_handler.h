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
#if defined(__APPLE__) || defined(__linux__)
#include <signal.h>
#endif

namespace domovoy {
namespace crash {

class CrashHandler {
public:
    static CrashHandler& GetInstance();

    void Install();
    void Uninstall();

private:
    CrashHandler() = default;
    ~CrashHandler() = default;

    bool installed_{false};

#if defined(__APPLE__) || defined(__linux__)
    static void HandleCrashSignal(int sig, siginfo_t* info, void* ucontext);
#elif defined(_WIN32)
    void* veh_handle_{nullptr};
    static long __stdcall VectoredExceptionHandler(struct _EXCEPTION_POINTERS* ep);
#endif
};

} // namespace crash
} // namespace domovoy
