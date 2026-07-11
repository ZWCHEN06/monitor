#include "index_fund_monitor/data/mock_price_provider.hpp"
#include "index_fund_monitor/data/json_util.hpp"

#include <filesystem>

MockPriceProvider::MockPriceProvider(std::string fixture_dir)
    : fixture_dir_(std::move(fixture_dir)) {}

void MockPriceProvider::set_fixture(const std::string& filename) {
    fixture_file_ = filename;
}

void MockPriceProvider::set_simulate_network_error(bool v) {
    simulate_network_error_ = v;
}

void MockPriceProvider::set_simulate_format_error(bool v) {
    simulate_format_error_ = v;
}

PriceResult MockPriceProvider::fetch(const std::string& symbol) {
    PriceResult result;

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
        DailyPrice price;
        price.symbol = symbol;
        price.trade_date = read_json_string(item, "trade_date").value_or("");
        price.open_price = read_json_double(item, "open");
        price.close_price = read_json_double(item, "close");
        price.high_price = read_json_double(item, "high");
        price.low_price = read_json_double(item, "low");
        price.volume = read_json_double(item, "volume");
        price.amount = read_json_double(item, "amount");
        price.source = read_json_string(item, "source").value_or("mock");
        price.collected_at = read_json_string(item, "collected_at").value_or("");

        if (price.trade_date.empty()) continue;

        result.prices.push_back(std::move(price));
    }

    result.success = true;
    return result;
}