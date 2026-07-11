#include "index_fund_monitor/data/mock_index_valuation_provider.hpp"
#include "index_fund_monitor/data/json_util.hpp"

#include <filesystem>

MockIndexValuationProvider::MockIndexValuationProvider(std::string fixture_dir)
    : fixture_dir_(std::move(fixture_dir)) {}

void MockIndexValuationProvider::set_fixture(const std::string& filename) {
    fixture_file_ = filename;
}

void MockIndexValuationProvider::set_simulate_network_error(bool v) {
    simulate_network_error_ = v;
}

void MockIndexValuationProvider::set_simulate_format_error(bool v) {
    simulate_format_error_ = v;
}

IndexValuationResult MockIndexValuationProvider::fetch(const std::string& benchmark_code) {
    IndexValuationResult result;

    if (simulate_network_error_) {
        result.error.message = "模拟网络错误：无法连接到服务器";
        result.error.is_network_error = true;
        return result;
    }

    auto path = (std::filesystem::path(fixture_dir_) / fixture_file_).string();
    auto json = load_json_file(path);

    if (!json.has_value()) {
        result.error.message = "模拟错误：无法读取文件 " + path;
        result.error.is_format_error = true;
        return result;
    }

    if (simulate_format_error_) {
        result.error.message = "模拟格式错误：JSON解析失败";
        result.error.is_format_error = true;
        return result;
    }

    if (!json->is_array()) {
        result.error.message = "数据格式错误：期望数组";
        result.error.is_format_error = true;
        return result;
    }

    for (const auto& item : *json) {
        IndexValuation val;
        val.benchmark_code = benchmark_code;
        val.valuation_date = read_json_string(item, "valuation_date").value_or("");
        val.pe_ttm = read_json_double(item, "pe_ttm");
        val.pb = read_json_double(item, "pb");
        val.dividend_yield = read_json_double(item, "dividend_yield");
        val.source = read_json_string(item, "source").value_or("mock");
        val.collected_at = read_json_string(item, "collected_at").value_or("");

        if (val.valuation_date.empty()) continue;

        result.valuations.push_back(std::move(val));
    }

    result.success = true;
    return result;
}