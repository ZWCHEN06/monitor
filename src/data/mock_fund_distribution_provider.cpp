#include "index_fund_monitor/data/mock_fund_distribution_provider.hpp"
#include "index_fund_monitor/data/json_util.hpp"

#include <filesystem>

MockFundDistributionProvider::MockFundDistributionProvider(std::string fixture_dir)
    : fixture_dir_(std::move(fixture_dir)) {}

void MockFundDistributionProvider::set_fixture(const std::string& filename) {
    fixture_file_ = filename;
}

void MockFundDistributionProvider::set_simulate_network_error(bool v) {
    simulate_network_error_ = v;
}

void MockFundDistributionProvider::set_simulate_format_error(bool v) {
    simulate_format_error_ = v;
}

FundDistributionResult MockFundDistributionProvider::fetch(const std::string& symbol) {
    FundDistributionResult result;

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
        FundDistribution dist;
        dist.symbol = symbol;
        dist.amount_per_share = read_json_double(item, "amount_per_share");
        dist.ex_date = read_json_string(item, "ex_date").value_or("");
        dist.record_date = read_json_string(item, "record_date").value_or("");
        dist.pay_date = read_json_string(item, "pay_date").value_or("");
        dist.currency = read_json_string(item, "currency").value_or("CNY");
        dist.source = read_json_string(item, "source").value_or("mock");

        if (dist.ex_date.empty()) continue;

        result.distributions.push_back(std::move(dist));
    }

    result.success = true;
    return result;
}