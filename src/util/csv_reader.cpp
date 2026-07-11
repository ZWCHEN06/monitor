#include "index_fund_monitor/util/csv_reader.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

bool is_bom(const char* data) {
    const unsigned char* u = reinterpret_cast<const unsigned char*>(data);
    return u[0] == 0xEF && u[1] == 0xBB && u[2] == 0xBF;
}

} // namespace

std::string CsvReader::remove_bom(std::string_view text) {
    if (text.size() >= 3 && is_bom(text.data())) {
        return std::string(text.substr(3));
    }
    return std::string(text);
}

std::vector<std::string> CsvReader::split_line(std::string_view line) const {
    std::vector<std::string> fields;
    std::string field;
    bool in_quotes = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        if (in_quotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field += '"';
                    ++i;
                } else {
                    in_quotes = false;
                }
            } else {
                field += c;
            }
        } else {
            if (c == '"') {
                in_quotes = true;
            } else if (c == ',') {
                fields.push_back(std::move(field));
                field.clear();
            } else {
                field += c;
            }
        }
    }

    fields.push_back(std::move(field));
    return fields;
}

CsvParseResult CsvReader::parse_string(std::string_view csv_str) {
    CsvParseResult result;

    auto cleaned = remove_bom(csv_str);
    std::vector<std::string> lines;
    std::size_t start = 0;

    // Split by newlines, handling CRLF
    for (std::size_t i = 0; i <= cleaned.size(); ++i) {
        if (i == cleaned.size() || cleaned[i] == '\n') {
            auto line = cleaned.substr(start, i - start);
            // Remove trailing CR
            if (!line.empty() && line.back() == '\r') {
                line = std::string(line.substr(0, line.size() - 1));
            }
            lines.push_back(line);
            start = i + 1;
        }
    }

    if (lines.empty()) {
        result.error.message = "CSV内容为空";
        return result;
    }

    // Remove empty trailing lines
    while (!lines.empty() && lines.back().empty()) {
        lines.pop_back();
    }

    if (lines.empty()) {
        result.error.message = "CSV内容为空";
        return result;
    }

    // Parse headers
    result.headers = split_line(lines[0]);

    // Parse data rows
    for (std::size_t i = 1; i < lines.size(); ++i) {
        if (lines[i].empty()) continue;
        auto fields = split_line(lines[i]);
        result.rows.push_back(std::move(fields));
    }

    result.success = true;
    return result;
}

CsvParseResult CsvReader::parse_file(const std::string& path) {
    CsvParseResult result;

    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        result.error.message = "文件不存在: " + path;
        return result;
    }

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            result.error.message = "无法打开文件: " + path;
            return result;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return parse_string(buffer.str());
    } catch (const std::exception& e) {
        result.error.message = "读取文件错误: " + std::string(e.what());
        return result;
    }
}