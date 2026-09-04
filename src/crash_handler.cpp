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

#include "domovoy/crash_handler.h"
#include "domovoy/core.h"
#include <cpptrace/cpptrace.hpp>
#include <iostream>

#if defined(__APPLE__) || defined(__linux__)
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace domovoy {
namespace crash {

#if defined(__APPLE__) || defined(__linux__)
static struct sigaction old_actions[32];

void CrashHandler::HandleCrashSignal(int sig, siginfo_t* info, void* ucontext) {
    // Note: cpptrace::generate_trace() allocates memory. In a real crash handler,
    // this is dangerous if the heap is corrupted. cpptrace does have a safe unwind,
    // but for this prototype we'll attempt it.
    auto trace = cpptrace::generate_trace();
    
    isolated_json<> report;
    report["type"] = "crash";
    report["signal"] = sig;
    
    auto& frames = report["backtrace"] = isolated_json<>::array();
    
    for (const auto& frame : trace.frames) {
        isolated_json<> frame_json;
        frame_json["symbol"] = frame.symbol.empty() ? "<unknown>" : frame.symbol.c_str();
        frame_json["filename"] = frame.filename.empty() ? "<unknown>" : frame.filename.c_str();
        frame_json["line"] = frame.line.value_or(0);
        frames.push_back(frame_json);
    }

    Reporter* reporter = DomovoyCore::GetReporter();
    if (reporter) {
        reporter->ReportCrash(report);
    }

    // Call old handler
    if (old_actions[sig].sa_flags & SA_SIGINFO) {
        if (old_actions[sig].sa_sigaction) {
            old_actions[sig].sa_sigaction(sig, info, ucontext);
        }
    } else {
        if (old_actions[sig].sa_handler == SIG_DFL) {
            signal(sig, SIG_DFL);
            raise(sig);
        } else if (old_actions[sig].sa_handler != SIG_IGN) {
            old_actions[sig].sa_handler(sig);
        }
    }
}
#elif defined(_WIN32)
long __stdcall CrashHandler::UnhandledExceptionFilter(struct _EXCEPTION_POINTERS* ep) {
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ||
        ep->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION ||
        ep->ExceptionRecord->ExceptionCode == EXCEPTION_INT_DIVIDE_BY_ZERO ||
        ep->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW) {
        
        auto trace = cpptrace::generate_trace();
        
        isolated_json<> report;
        report["type"] = "crash";
        report["signal"] = ep->ExceptionRecord->ExceptionCode;
        
        auto& frames = report["backtrace"] = isolated_json<>::array();
        
        for (const auto& frame : trace.frames) {
            isolated_json<> frame_json;
            frame_json["symbol"] = frame.symbol.empty() ? "<unknown>" : frame.symbol.c_str();
            frame_json["filename"] = frame.filename.empty() ? "<unknown>" : frame.filename.c_str();
            frame_json["line"] = frame.line.value_or(0);
            frames.push_back(frame_json);
        }

        Reporter* reporter = DomovoyCore::GetReporter();
        if (reporter) {
            reporter->ReportCrash(report);
        }
    }
    
    if (CrashHandler::GetInstance().previous_filter_) {
        return CrashHandler::GetInstance().previous_filter_(ep);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

CrashHandler& CrashHandler::GetInstance() {
    static CrashHandler instance;
    return instance;
}

void CrashHandler::Install() {
    if (installed_) return;
    installed_ = true;

#if defined(__APPLE__) || defined(__linux__)
    // Setup alternate signal stack
    stack_t altstack;
    altstack.ss_sp = domovoy::memory::IsolatedAllocator::GetInstance().Allocate(SIGSTKSZ);
    altstack.ss_size = SIGSTKSZ;
    altstack.ss_flags = 0;
    sigaltstack(&altstack, nullptr);

    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sa.sa_sigaction = &CrashHandler::HandleCrashSignal;
    sigemptyset(&sa.sa_mask);

    int signals[] = {SIGSEGV, SIGABRT, SIGILL, SIGFPE, SIGBUS};
    for (int sig : signals) {
        sigaction(sig, &sa, &old_actions[sig]);
    }
#elif defined(_WIN32)
    // SetUnhandledExceptionFilter runs *after* all application SEH blocks, ensuring we only
    // catch true crashes, avoiding interference with normal control flow access violations.
    if (!installed_) {
        previous_filter_ = SetUnhandledExceptionFilter(UnhandledExceptionFilter);
    }
#endif
}

void CrashHandler::Uninstall() {
    if (!installed_) return;
    installed_ = false;

#if defined(__APPLE__) || defined(__linux__)
    int signals[] = {SIGSEGV, SIGABRT, SIGILL, SIGFPE, SIGBUS};
    for (int sig : signals) {
        sigaction(sig, &old_actions[sig], nullptr);
    }
    
    stack_t altstack;
    altstack.ss_flags = SS_DISABLE;
    sigaltstack(&altstack, nullptr);
#elif defined(_WIN32)
    SetUnhandledExceptionFilter(previous_filter_);
    previous_filter_ = nullptr;
#endif
}

} // namespace crash
} // namespace domovoy
