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

#include <string>
#include <vector>
#include <memory>
#include "domovoy/allocator.h"
#include <nlohmann/json.hpp>

namespace domovoy {

// We use the nlohmann::basic_json but parameterized with our StlAllocator
// so that all JSON DOM allocations come from our isolated memory pool, 
// protecting us from application heap corruption during crash reporting.
template<template<typename U, typename V, typename... Args> class ObjectType = std::map,
         template<typename U, typename... Args> class ArrayType = std::vector,
         class StringType = std::basic_string<char, std::char_traits<char>, memory::StlAllocator<char>>,
         class BooleanType = bool,
         class NumberIntegerType = std::int64_t,
         class NumberUnsignedType = std::uint64_t,
         class NumberFloatType = double,
         template<typename U> class AllocatorType = memory::StlAllocator,
         template<typename T, typename S1, typename... Args> class JSONSerializer = nlohmann::adl_serializer>
using isolated_json = nlohmann::basic_json<ObjectType, ArrayType, StringType, BooleanType,
                                           NumberIntegerType, NumberUnsignedType, NumberFloatType,
                                           AllocatorType, JSONSerializer>;

using isolated_string = std::basic_string<char, std::char_traits<char>, memory::StlAllocator<char>>;

class Reporter {
public:
    virtual ~Reporter() = default;

    // Called when the application is shutting down normally to report leaks.
    virtual void ReportLeaks(const isolated_json<>& leak_data) = 0;

    // Called when a high CPU thread is detected.
    virtual void ReportHighCpu(const isolated_json<>& report) = 0;
    virtual void ReportIoLeak(const isolated_json<>& report) = 0;

    // Called when a fatal crash is intercepted.
    virtual void ReportCrash(const isolated_json<>& crash_data) = 0;
};

// Default JSON File reporter implementation
class JsonFileReporter : public Reporter {
public:
    JsonFileReporter(isolated_string output_dir);
    ~JsonFileReporter() override = default;

    void ReportLeaks(const isolated_json<>& leak_data) override;
    void ReportHighCpu(const isolated_json<>& cpu_data) override;
    void ReportIoLeak(const isolated_json<>& report) override;
    void ReportCrash(const isolated_json<>& crash_data) override;

private:
    void WriteToFile(const isolated_string& prefix, const isolated_json<>& data);
    isolated_string output_dir_;
};

} // namespace domovoy
