#ifndef INDEX_FUND_MONITOR_UTIL_JSON_READER_HPP
#define INDEX_FUND_MONITOR_UTIL_JSON_READER_HPP

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

struct JsonReadError {
    std::string message;
    std::string field_path;
};

struct JsonParseResult {
    bool success = false;
    JsonReadError error;
};

class JsonReader {
public:
    JsonReader();

    JsonParseResult parse_string(std::string_view json_str);
    JsonParseResult parse_file(const std::string& path);

    bool is_object() const;
    bool is_array() const;
    std::size_t array_size() const;

    bool has_field(const std::string& key) const;
    std::optional<std::string> read_string(const std::string& key);
    std::optional<double> read_double(const std::string& key);
    std::optional<int> read_int(const std::string& key);
    std::optional<bool> read_bool(const std::string& key);

    std::optional<std::string> read_string_at(std::size_t index);
    std::optional<double> read_double_at(std::size_t index);

    JsonReadError last_error() const;
    std::string last_error_with_path(const std::string& key) const;

    const nlohmann::json& root() const;

private:
    nlohmann::json root_;
    JsonReadError last_error_;
    bool has_value_ = false;
};

#endif