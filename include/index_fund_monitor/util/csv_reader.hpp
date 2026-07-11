#ifndef INDEX_FUND_MONITOR_UTIL_CSV_READER_HPP
#define INDEX_FUND_MONITOR_UTIL_CSV_READER_HPP

#include <string>
#include <string_view>
#include <vector>

struct CsvReadError {
    std::string message;
    int line = 0;
};

struct CsvParseResult {
    bool success = false;
    CsvReadError error;
    std::vector<std::string> headers;
    std::vector<std::vector<std::string>> rows;
};

class CsvReader {
public:
    CsvParseResult parse_string(std::string_view csv_str);
    CsvParseResult parse_file(const std::string& path);

private:
    std::vector<std::string> split_line(std::string_view line) const;
    static std::string remove_bom(std::string_view text);
};

#endif