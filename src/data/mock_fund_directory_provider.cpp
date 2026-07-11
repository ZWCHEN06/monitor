#include "index_fund_monitor/data/mock_fund_directory_provider.hpp"
#include "index_fund_monitor/data/json_util.hpp"

#include <filesystem>
#include <fstream>

MockFundDirectoryProvider::MockFundDirectoryProvider(std::string fixture_dir)
    : fixture_dir_(std::move(fixture_dir)) {}

void MockFundDirectoryProvider::set_fixture(const std::string& filename) {
    fixture_file_ = filename;
}

void MockFundDirectoryProvider::set_simulate_network_error(bool v) {
    simulate_network_error_ = v;
}

void MockFundDirectoryProvider::set_simulate_format_error(bool v) {
    simulate_format_error_ = v;
}

FundDirectoryResult MockFundDirectoryProvider::fetch_all() {
    FundDirectoryResult result;

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
        FundRecord rec;
        auto sym = read_json_string(item, "symbol");
        auto exch = read_json_string(item, "exchange");
        auto name = read_json_string(item, "name");

        if (!sym.has_value() || !exch.has_value() || !name.has_value()) {
            continue;
        }

        rec.symbol = sym.value();
        rec.exchange = (exch.value() == "SSE") ? Exchange::SSE : Exchange::SZSE;
        rec.name = name.value();

        auto mgr = read_json_string(item, "manager");
        if (mgr.has_value()) rec.manager = mgr.value();

        auto ft = read_json_string(item, "fund_type");
        if (ft.has_value()) rec.fund_type = ft.value();

        auto ls = read_json_string(item, "listing_status");
        if (ls.has_value()) rec.listing_status = ls.value();

        rec.currency = "CNY";
        result.funds.push_back(std::move(rec));
    }

    result.success = true;
    return result;
}