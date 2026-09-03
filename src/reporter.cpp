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

#include "domovoy/reporter.h"
#include <fstream>
#include <chrono>

namespace domovoy {

JsonFileReporter::JsonFileReporter(isolated_string output_dir)
    : output_dir_(std::move(output_dir)) {
}

void JsonFileReporter::WriteToFile(const isolated_string& prefix, const isolated_json<>& data) {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    // We use standard string for filename manipulation here because it's passed to std::ofstream,
    // which expects a standard string or const char*. We are assuming this is somewhat safe,
    // though in a hard crash, we might want to bypass std::ofstream and use direct POSIX/Windows IO.
    std::string filename = std::string(output_dir_.c_str()) + "/" + std::string(prefix.c_str()) + "_" + std::to_string(ms) + ".json";

    std::ofstream file(filename);
    if (file.is_open()) {
        file << data.dump(4);
    }
}

void JsonFileReporter::ReportLeaks(const isolated_json<>& leak_data) {
    WriteToFile(isolated_string("domovoy_leaks"), leak_data);
}

void JsonFileReporter::ReportHighCpu(const isolated_json<>& cpu_data) {
    WriteToFile(isolated_string("domovoy_cpu"), cpu_data);
}

void JsonFileReporter::ReportIoLeak(const isolated_json<>& report) {
    WriteToFile(isolated_string("domovoy_io_leak"), report);
}

void JsonFileReporter::ReportCrash(const isolated_json<>& crash_data) {
    WriteToFile(isolated_string("domovoy_crash"), crash_data);
}

} // namespace domovoy
