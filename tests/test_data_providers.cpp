#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "index_fund_monitor/data/mock_fund_directory_provider.hpp"
#include "index_fund_monitor/data/mock_index_valuation_provider.hpp"
#include "index_fund_monitor/data/mock_fund_distribution_provider.hpp"
#include "index_fund_monitor/data/mock_price_provider.hpp"

static int failures = 0;
static int tests = 0;

#define CHECK(cond, msg) do { ++tests; if (!(cond)) { std::cerr << "FAIL [" << tests << "]: " << msg << std::endl; ++failures; } else { std::cout << "OK   [" << tests << "]: " << msg << std::endl; } } while(0)
#define CHECK_EQ(a, b, msg) CHECK((a) == (b), msg)

namespace {

std::string fixture_dir() {
    return (std::filesystem::path(__FILE__).parent_path() / "fixtures").string();
}

void test_fund_directory_success() {
    MockFundDirectoryProvider provider(fixture_dir());
    auto result = provider.fetch_all();

    CHECK(result.success, "fund directory success");
    CHECK_EQ(result.funds.size(), 3u, "3 funds returned");
    if (result.funds.size() >= 3) {
        CHECK_EQ(result.funds[0].symbol, "510300", "first fund symbol");
        CHECK_EQ(result.funds[0].name, "沪深300ETF", "first fund name");
        CHECK_EQ(result.funds[0].exchange, Exchange::SSE, "first fund exchange SSE");
        CHECK_EQ(result.funds[1].symbol, "510050", "second fund symbol");
        CHECK_EQ(result.funds[2].symbol, "159915", "third fund symbol");
        CHECK_EQ(result.funds[2].exchange, Exchange::SZSE, "third fund exchange SZSE");
    }
}

void test_fund_directory_missing_fields() {
    MockFundDirectoryProvider provider(fixture_dir());
    provider.set_fixture("fund_directory_missing_fields.json");
    auto result = provider.fetch_all();

    CHECK(result.success, "missing fields still success");
    CHECK_EQ(result.funds.size(), 1u, "1 fund with required fields present");
    if (!result.funds.empty()) {
        CHECK(result.funds[0].manager.empty(), "manager empty when missing");
        CHECK(result.funds[0].fund_type.empty(), "fund_type empty when missing");
    }
}

void test_fund_directory_empty() {
    MockFundDirectoryProvider provider(fixture_dir());
    provider.set_fixture("fund_directory_empty.json");
    auto result = provider.fetch_all();

    CHECK(result.success, "empty list success");
    CHECK(result.funds.empty(), "no funds returned");
}

void test_fund_directory_network_error() {
    MockFundDirectoryProvider provider(fixture_dir());
    provider.set_simulate_network_error(true);
    auto result = provider.fetch_all();

    CHECK(!result.success, "network error returns failure");
    CHECK(result.error.is_network_error, "network error flagged");
    CHECK(!result.error.message.empty(), "error message present");
}

void test_fund_directory_format_error() {
    MockFundDirectoryProvider provider(fixture_dir());
    provider.set_fixture("malformed.json");
    provider.set_simulate_format_error(true);
    auto result = provider.fetch_all();

    CHECK(!result.success, "format error returns failure");
    CHECK(result.error.is_format_error, "format error flagged");
}

void test_index_valuation_success() {
    MockIndexValuationProvider provider(fixture_dir());
    auto result = provider.fetch("000300");

    CHECK(result.success, "valuation success");
    CHECK_EQ(result.valuations.size(), 2u, "2 valuations returned");
    if (!result.valuations.empty()) {
        CHECK_EQ(result.valuations[0].valuation_date, "2026-07-10", "valuation date");
        CHECK(result.valuations[0].pe_ttm.has_value(), "pe_ttm present");
        CHECK_EQ(result.valuations[0].pe_ttm.value(), 12.5, "pe_ttm value");
        CHECK(result.valuations[0].pb.has_value(), "pb present");
        CHECK(result.valuations[0].dividend_yield.has_value(), "dividend_yield present");
    }
}

void test_index_valuation_partial() {
    MockIndexValuationProvider provider(fixture_dir());
    provider.set_fixture("index_valuation_partial.json");
    auto result = provider.fetch("000300");

    CHECK(result.success, "partial valuation success");
    CHECK_EQ(result.valuations.size(), 1u, "1 valuation");
    if (!result.valuations.empty()) {
        CHECK(!result.valuations[0].pe_ttm.has_value(), "pe_ttm null");
        CHECK(result.valuations[0].pb.has_value(), "pb present");
        CHECK(!result.valuations[0].dividend_yield.has_value(), "dividend_yield null");
    }
}

void test_index_valuation_network_error() {
    MockIndexValuationProvider provider(fixture_dir());
    provider.set_simulate_network_error(true);
    auto result = provider.fetch("000300");

    CHECK(!result.success, "valuation network error");
    CHECK(result.error.is_network_error, "network error flagged");
}

void test_fund_distribution_success() {
    MockFundDistributionProvider provider(fixture_dir());
    auto result = provider.fetch("510300");

    CHECK(result.success, "distribution success");
    CHECK_EQ(result.distributions.size(), 1u, "1 distribution");
    if (!result.distributions.empty()) {
        CHECK_EQ(result.distributions[0].symbol, "510300", "distribution symbol");
        CHECK(result.distributions[0].amount_per_share.has_value(), "amount_per_share present");
        CHECK_EQ(result.distributions[0].amount_per_share.value(), 0.50, "amount 0.50");
    }
}

void test_fund_distribution_network_error() {
    MockFundDistributionProvider provider(fixture_dir());
    provider.set_simulate_network_error(true);
    auto result = provider.fetch("510300");

    CHECK(!result.success, "distribution network error");
    CHECK(result.error.is_network_error, "network error flagged");
}

void test_malformed_json() {
    MockFundDirectoryProvider provider(fixture_dir());
    provider.set_fixture("malformed.json");
    auto result = provider.fetch_all();

    CHECK(!result.success, "malformed json fails");
    CHECK(result.error.is_format_error, "format error");
}

void test_no_such_fixture() {
    MockFundDirectoryProvider provider(fixture_dir());
    provider.set_fixture("nonexistent.json");
    auto result = provider.fetch_all();

    CHECK(!result.success, "nonexistent file fails");
}

void test_provider_interface_polymorphism() {
    // Verify that mocks can be used through interface pointers
    std::unique_ptr<IFundDirectoryProvider> provider =
        std::make_unique<MockFundDirectoryProvider>(fixture_dir());

    auto result = provider->fetch_all();
    CHECK(result.success, "polymorphic call works");
    CHECK_EQ(result.funds.size(), 3u, "polymorphic returns data");
}

} // namespace

int main() {
    test_fund_directory_success();
    test_fund_directory_missing_fields();
    test_fund_directory_empty();
    test_fund_directory_network_error();
    test_fund_directory_format_error();
    test_index_valuation_success();
    test_index_valuation_partial();
    test_index_valuation_network_error();
    test_fund_distribution_success();
    test_fund_distribution_network_error();
    test_malformed_json();
    test_no_such_fixture();
    test_provider_interface_polymorphism();

    std::cout << "\n=== " << tests << " tests, " << failures << " failures ===" << std::endl;
    return failures > 0 ? 1 : 0;
}