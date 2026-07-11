#ifndef INDEX_FUND_MONITOR_DATA_JSON_UTIL_HPP
#define INDEX_FUND_MONITOR_DATA_JSON_UTIL_HPP

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

inline std::optional<nlohmann::json> load_json_file(const std::string& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return std::nullopt;
    try {
        std::ifstream file(path);
        if (!file.is_open()) return std::nullopt;
        nlohmann::json j;
        file >> j;
        return j;
    } catch (...) {
        return std::nullopt;
    }
}

inline std::optional<std::string> read_json_string(const nlohmann::json& j, const std::string& key) {
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) return std::nullopt;
    try { return it->get<std::string>(); } catch (...) { return std::nullopt; }
}

inline std::optional<double> read_json_double(const nlohmann::json& j, const std::string& key) {
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) return std::nullopt;
    if (!it->is_number()) return std::nullopt;
    try { return it->get<double>(); } catch (...) { return std::nullopt; }
}

#endif