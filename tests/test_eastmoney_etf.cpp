#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "index_fund_monitor/data/eastmoney_etf_provider.hpp"
#include "index_fund_monitor/models/models.hpp"
#include "index_fund_monitor/net/http_client.hpp"

static int failures = 0;
static int tests = 0;

#define CHECK(cond, msg) do { \
    ++tests; \
    if (!(cond)) { \
        std::cerr << "FAIL [" << tests << "]: " << msg << std::endl; \
        ++failures; \
    } else { \
        std::cout << "OK   [" << tests << "]: " << msg << std::endl; \
    } \
} while(0)

#define CHECK_EQ(a, b, msg) CHECK((a) == (b), msg)

namespace {

void test_parse_success() {
    auto http = std::make_shared<MockHttpClient>();
    EastMoneyETFProvider provider(http);

    std::string json = R"({
        "rc": 0,
        "data": {
            "total": 3,
            "diff": [
                {"f12":"510300","f14":"沪深300ETF华泰柏瑞"},
                {"f12":"510050","f14":"上证50ETF华夏"},
                {"f12":"159915","f14":"创业板ETF易方达"}
            ]
        }
    })";

    auto result = provider.parse_response(json);

    CHECK(result.success, "parse success");
    CHECK_EQ(result.funds.size(), 3u, "3 funds parsed");
    CHECK_EQ(result.funds[0].symbol, "510300", "first code");
    CHECK_EQ(result.funds[0].name, "沪深300ETF华泰柏瑞", "first name");
    CHECK_EQ(std::string(to_string(result.funds[0].exchange)), "SSE", "510300 -> SSE");
    CHECK_EQ(result.funds[1].symbol, "510050", "second code");
    CHECK_EQ(result.funds[1].name, "上证50ETF华夏", "second name");
    CHECK_EQ(result.funds[2].symbol, "159915", "third code");
    CHECK_EQ(std::string(to_string(result.funds[2].exchange)), "SZSE", "159915 -> SZSE");
}

void test_parse_manager_extracted() {
    auto http = std::make_shared<MockHttpClient>();
    EastMoneyETFProvider provider(http);

    std::string json = R"({
        "rc": 0,
        "data": {
            "total": 2,
            "diff": [
                {"f12":"510300","f14":"沪深300ETF华泰柏瑞"},
                {"f12":"510050","f14":"上证50ETF华夏"}
            ]
        }
    })";

    auto result = provider.parse_response(json);

    CHECK(result.success, "parse success");
    CHECK_EQ(result.funds[0].manager, "华泰柏瑞", "manager extracted from name");
    CHECK_EQ(result.funds[1].manager, "华夏", "manager extracted from name");
}

void test_parse_no_manager_in_name() {
    auto http = std::make_shared<MockHttpClient>();
    EastMoneyETFProvider provider(http);

    std::string json = R"({
        "rc": 0,
        "data": {
            "total": 1,
            "diff": [
                {"f12":"510300","f14":"沪深300ETF"}
            ]
        }
    })";

    auto result = provider.parse_response(json);

    CHECK(result.success, "parse success");
    CHECK(result.funds[0].manager.empty(), "manager empty when not in name");
}

void test_parse_skip_non_a_share() {
    auto http = std::make_shared<MockHttpClient>();
    EastMoneyETFProvider provider(http);

    std::string json = R"({
        "rc": 0,
        "data": {
            "total": 3,
            "diff": [
                {"f12":"510300","f14":"沪深300ETF"},
                {"f12":"159915","f14":"创业板ETF"},
                {"f12":"999999","f14":"未知ETF"}
            ]
        }
    })";

    auto result = provider.parse_response(json);

    CHECK(result.success, "parse success");
    CHECK_EQ(result.funds.size(), 2u, "only 2 A-share ETFs (999999 skipped)");
    CHECK_EQ(result.funds[0].symbol, "510300", "510300 first (API order)");
    CHECK_EQ(result.funds[1].symbol, "159915", "159915 second");
}

void test_parse_rc_error() {
    auto http = std::make_shared<MockHttpClient>();
    EastMoneyETFProvider provider(http);

    std::string json = R"({
        "rc": -1,
        "data": null
    })";

    auto result = provider.parse_response(json);

    CHECK(!result.success, "parse fails with error rc");
    CHECK(result.error.is_format_error, "format error flagged");
}

void test_parse_malformed_json() {
    auto http = std::make_shared<MockHttpClient>();
    EastMoneyETFProvider provider(http);

    auto result = provider.parse_response("{invalid json}");

    CHECK(!result.success, "parse fails with malformed json");
    CHECK(result.error.is_format_error, "format error flagged");
}

void test_parse_missing_data() {
    auto http = std::make_shared<MockHttpClient>();
    EastMoneyETFProvider provider(http);

    std::string json = R"({"rc": 0})";

    auto result = provider.parse_response(json);

    CHECK(!result.success, "parse fails without data");
    CHECK(result.error.is_format_error, "format error flagged");
}

void test_parse_empty_diff() {
    auto http = std::make_shared<MockHttpClient>();
    EastMoneyETFProvider provider(http);

    std::string json = R"({
        "rc": 0,
        "data": {
            "total": 0,
            "diff": []
        }
    })";

    auto result = provider.parse_response(json);

    CHECK(result.success, "parse success with empty diff");
    CHECK(result.funds.empty(), "no funds returned");
}

void test_parse_missing_fields() {
    auto http = std::make_shared<MockHttpClient>();
    EastMoneyETFProvider provider(http);

    std::string json = R"({
        "rc": 0,
        "data": {
            "total": 2,
            "diff": [
                {"f12":"510300"},
                {"f14":"无名ETF"}
            ]
        }
    })";

    auto result = provider.parse_response(json);

    CHECK(result.success, "parse still succeeds");
    CHECK(result.funds.empty(), "no funds when fields are missing");
}

void test_fetch_with_fixture() {
    auto http = std::make_shared<MockHttpClient>();
    EastMoneyETFProvider provider(http);

    // Use the saved fixture file
    auto fixture_path = std::filesystem::absolute("tests/fixtures").string();
    provider.set_fixture_dir(fixture_path);

    auto result = provider.fetch_all();

    CHECK(result.success, "fixture fetch success");
    CHECK(!result.funds.empty(), "funds returned from fixture");
    CHECK(!result.data_date.empty(), "data_date set");
    CHECK(!result.collected_at.empty(), "collected_at set");
}

void test_network_error() {
    auto http = std::make_shared<MockHttpClient>();
    EastMoneyETFProvider provider(http);
    provider.set_simulate_network_error(true);

    auto result = provider.fetch_all();

    CHECK(!result.success, "fetch fails with network error");
    CHECK(result.error.is_network_error, "network error flagged");
}

void test_format_error() {
    auto http = std::make_shared<MockHttpClient>();
    EastMoneyETFProvider provider(http);
    provider.set_simulate_format_error(true);

    auto result = provider.parse_response("[]");

    CHECK(!result.success, "parse fails with format error");
    CHECK(result.error.is_format_error, "format error flagged");
}

} // namespace

int main() {
    test_parse_success();
    test_parse_manager_extracted();
    test_parse_no_manager_in_name();
    test_parse_skip_non_a_share();
    test_parse_rc_error();
    test_parse_malformed_json();
    test_parse_missing_data();
    test_parse_empty_diff();
    test_parse_missing_fields();
    test_fetch_with_fixture();
    test_network_error();
    test_format_error();

    std::cout << "\n=== " << tests << " tests, " << failures << " failures ===" << std::endl;
    return failures > 0 ? 1 : 0;
}