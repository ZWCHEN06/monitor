#include "index_fund_monitor/data/eastmoney_etf_provider.hpp"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#include "index_fund_monitor/data/json_util.hpp"
#include "index_fund_monitor/models/validation.hpp"
#include "index_fund_monitor/net/http_client.hpp"

namespace {

std::string now_iso() {
    std::time_t t = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::gmtime(&t));
    return buf;
}

std::string today_date() {
    std::time_t t = std::time(nullptr);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::gmtime(&t));
    return buf;
}

FundRecord parse_item(const nlohmann::json& item) {
    FundRecord rec;

    auto code = read_json_string(item, "f12");
    auto name = read_json_string(item, "f14");

    if (!code.has_value() || !name.has_value()) {
        return rec;
    }

    rec.symbol = code.value();
    rec.name = name.value();

    auto exchange = deduce_exchange(rec.symbol);
    if (exchange.has_value()) {
        rec.exchange = exchange.value();
    }

    // Extract fund manager from name if possible
    // Names like "沪深300ETF华泰柏瑞" → manager is "华泰柏瑞"
    // Names like "科创50ETF国泰" → manager is "国泰"
    auto etf_pos = rec.name.find("ETF");
    if (etf_pos != std::string::npos && etf_pos + 3 < rec.name.size()) {
        rec.manager = rec.name.substr(etf_pos + 3);
    }

    rec.currency = "CNY";
    return rec;
}

} // namespace

EastMoneyETFProvider::EastMoneyETFProvider(std::shared_ptr<IHttpClient> http_client)
    : http_client_(std::move(http_client)) {}

void EastMoneyETFProvider::set_fixture_dir(const std::string& dir) {
    fixture_dir_ = dir;
    use_fixture_ = true;
}

void EastMoneyETFProvider::set_simulate_network_error(bool v) {
    simulate_network_error_ = v;
}

void EastMoneyETFProvider::set_simulate_format_error(bool v) {
    simulate_format_error_ = v;
}

ETFDirectoryResult EastMoneyETFProvider::parse_response(const std::string& json_body) {
    ETFDirectoryResult result;
    std::string collected = now_iso();

    if (simulate_format_error_) {
        result.error.message = "模拟格式错误";
        result.error.is_format_error = true;
        return result;
    }

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(json_body);
    } catch (...) {
        result.error.message = "JSON解析失败：无效的JSON格式";
        result.error.is_format_error = true;
        return result;
    }

    // Check return code
    auto rc = read_json_double(j, "rc");
    if (!rc.has_value() || rc.value() != 0.0) {
        result.error.message = "API返回错误代码";
        result.error.is_format_error = true;
        return result;
    }

    auto data = j.find("data");
    if (data == j.end() || !data->is_object()) {
        result.error.message = "响应缺少data字段";
        result.error.is_format_error = true;
        return result;
    }

    auto diff = data->find("diff");
    if (diff == data->end() || !diff->is_array()) {
        result.error.message = "响应缺少diff数组";
        result.error.is_format_error = true;
        return result;
    }

    for (const auto& item : *diff) {
        if (!item.is_object()) continue;

        auto rec = parse_item(item);
        if (rec.symbol.empty()) continue;

        // Skip non-A-share stock ETFs by exchange check
        auto exchange = deduce_exchange(rec.symbol);
        if (!exchange.has_value()) continue;

        rec.exchange = exchange.value();
        result.funds.push_back(std::move(rec));
    }

    result.success = true;
    result.data_date = today_date();
    result.collected_at = collected;
    return result;
}

ETFDirectoryResult EastMoneyETFProvider::fetch_all() {
    ETFDirectoryResult result;

    if (simulate_network_error_) {
        result.error.message = "模拟网络错误：无法连接到服务器";
        result.error.is_network_error = true;
        return result;
    }

    if (use_fixture_) {
        auto path = (std::filesystem::path(fixture_dir_) / "eastmoney_etf_market.json").string();
        auto json = load_json_file(path);
        if (!json.has_value()) {
            result.error.message = "无法读取fixture文件：" + path;
            result.error.is_format_error = true;
            return result;
        }
        return parse_response(json->dump());
    }

    // Real HTTP request
    HttpRequestOptions opts;
    opts.url = "https://push2.eastmoney.com/api/qt/clist/get"
               "?cb=&pn=1&pz=2000&po=1&np=1"
               "&ut=bd1d9ddb04089700cf9c27f6f7426281"
               "&fltt=2&invt=2&fid=f12"
               "&fs=b:MK0021,b:MK0022,b:MK0023,b:MK0024"
               "&fields=f12,f14"
               "&_=" + std::to_string(std::time(nullptr));
    opts.user_agent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";
    opts.connect_timeout = std::chrono::milliseconds(10000);
    opts.total_timeout = std::chrono::milliseconds(30000);
    opts.max_retries = 2;
    opts.max_response_size = 1024 * 1024; // 1MB

    auto response = http_client_->get(opts);
    if (!response.success) {
        result.error.message = response.error_message;
        result.error.is_network_error = true;
        return result;
    }

    if (response.status_code != 200) {
        result.error.message = "HTTP " + std::to_string(response.status_code);
        result.error.is_network_error = true;
        return result;
    }

    return parse_response(response.body);
}