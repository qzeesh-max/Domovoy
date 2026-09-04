#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>
#include <gtest/gtest.h>
#include <iostream>

inline void CleanUpOldReports() {
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(".", ec)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            if (filename.find("domovoy_") == 0 && filename.find(".json") != std::string::npos) {
                std::filesystem::remove(entry.path(), ec);
            }
        }
    }
}

inline nlohmann::json FindAndParseReport(const std::string& prefix) {
    nlohmann::json result;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(".", ec)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            if (filename.find(prefix) == 0 && filename.find(".json") != std::string::npos) {
                std::ifstream f(entry.path());
                if (f.is_open()) {
                    try {
                        result = nlohmann::json::parse(f);
                    } catch (const std::exception& e) {
                        std::cerr << "Failed to parse JSON file " << filename << ": " << e.what() << std::endl;
                    }
                    f.close();
                }
                std::filesystem::remove(entry.path(), ec);
                return result;
            }
        }
    }
    return result;
}
