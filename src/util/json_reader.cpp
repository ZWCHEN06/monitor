#include "index_fund_monitor/util/json_reader.hpp"

#include <filesystem>
#include <fstream>

JsonReader::JsonReader() = default;

JsonParseResult JsonReader::parse_string(std::string_view json_str) {
    has_value_ = false;
    try {
        root_ = nlohmann::json::parse(json_str);
        has_value_ = true;
        return {true, {}};
    } catch (const nlohmann::json::parse_error& e) {
        last_error_.message = "JSON解析错误: " + std::string(e.what());
        last_error_.field_path.clear();
        return {false, {last_error_.message, ""}};
    }
}

JsonParseResult JsonReader::parse_file(const std::string& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        last_error_.message = "文件不存在: " + path;
        return {false, {last_error_.message, ""}};
    }
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            last_error_.message = "无法打开文件: " + path;
            return {false, {last_error_.message, ""}};
        }
        file >> root_;
        has_value_ = true;
        return {true, {}};
    } catch (const nlohmann::json::parse_error& e) {
        last_error_.message = "JSON解析错误: " + std::string(e.what());
        return {false, {last_error_.message, ""}};
    } catch (const std::exception& e) {
        last_error_.message = "读取文件错误: " + std::string(e.what());
        return {false, {last_error_.message, ""}};
    }
}

bool JsonReader::is_object() const {
    return has_value_ && root_.is_object();
}

bool JsonReader::is_array() const {
    return has_value_ && root_.is_array();
}

std::size_t JsonReader::array_size() const {
    if (!is_array()) return 0;
    return root_.size();
}

bool JsonReader::has_field(const std::string& key) const {
    if (!is_object()) return false;
    auto it = root_.find(key);
    return it != root_.end() && !it->is_null();
}

std::optional<std::string> JsonReader::read_string(const std::string& key) {
    if (!is_object()) return std::nullopt;
    auto it = root_.find(key);
    if (it == root_.end() || it->is_null()) return std::nullopt;
    if (!it->is_string()) {
        last_error_ = {"类型错误: 字段 '" + key + "' 不是字符串类型", key};
        return std::nullopt;
    }
    try { return it->get<std::string>(); } catch (...) { return std::nullopt; }
}

std::optional<double> JsonReader::read_double(const std::string& key) {
    if (!is_object()) return std::nullopt;
    auto it = root_.find(key);
    if (it == root_.end() || it->is_null()) return std::nullopt;
    if (!it->is_number()) {
        last_error_ = {"类型错误: 字段 '" + key + "' 不是数字类型", key};
        return std::nullopt;
    }
    try { return it->get<double>(); } catch (...) { return std::nullopt; }
}

std::optional<int> JsonReader::read_int(const std::string& key) {
    if (!is_object()) return std::nullopt;
    auto it = root_.find(key);
    if (it == root_.end() || it->is_null()) return std::nullopt;
    if (!it->is_number_integer()) {
        last_error_ = {"类型错误: 字段 '" + key + "' 不是整数类型", key};
        return std::nullopt;
    }
    try { return it->get<int>(); } catch (...) { return std::nullopt; }
}

std::optional<bool> JsonReader::read_bool(const std::string& key) {
    if (!is_object()) return std::nullopt;
    auto it = root_.find(key);
    if (it == root_.end() || it->is_null()) return std::nullopt;
    if (!it->is_boolean()) {
        last_error_ = {"类型错误: 字段 '" + key + "' 不是布尔类型", key};
        return std::nullopt;
    }
    try { return it->get<bool>(); } catch (...) { return std::nullopt; }
}

std::optional<std::string> JsonReader::read_string_at(std::size_t index) {
    if (!is_array() || index >= root_.size()) return std::nullopt;
    const auto& item = root_[index];
    if (item.is_null()) return std::nullopt;
    if (!item.is_string()) return std::nullopt;
    try { return item.get<std::string>(); } catch (...) { return std::nullopt; }
}

std::optional<double> JsonReader::read_double_at(std::size_t index) {
    if (!is_array() || index >= root_.size()) return std::nullopt;
    const auto& item = root_[index];
    if (item.is_null()) return std::nullopt;
    if (!item.is_number()) return std::nullopt;
    try { return item.get<double>(); } catch (...) { return std::nullopt; }
}

JsonReadError JsonReader::last_error() const {
    return last_error_;
}

std::string JsonReader::last_error_with_path(const std::string& key) const {
    if (!last_error_.field_path.empty()) {
        return last_error_.message;
    }
    return "字段 '" + key + "': " + last_error_.message;
}

const nlohmann::json& JsonReader::root() const {
    return root_;
}