#include <cstdlib>
#include <iostream>
#include <string>

#include "index_fund_monitor/models/models.hpp"
#include "index_fund_monitor/models/validation.hpp"

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

void test_etf_code_validation() {
    CHECK(is_valid_etf_code("510300"), "valid 6-digit code");
    CHECK(is_valid_etf_code("000001"), "valid 6-digit code starting with 0");
    CHECK(!is_valid_etf_code("51030"),  "5-digit code invalid");
    CHECK(!is_valid_etf_code("5103000"), "7-digit code invalid");
    CHECK(!is_valid_etf_code("51030X"), "non-digit char invalid");
    CHECK(!is_valid_etf_code(""),       "empty code invalid");
    CHECK(!is_valid_etf_code("abc123"), "non-digit chars invalid");
}

void test_market_validation() {
    CHECK(is_valid_market("CN"),   "CN is valid market");
    CHECK(!is_valid_market("HK"),  "HK is not supported");
    CHECK(!is_valid_market("US"),  "US is not supported");
    CHECK(!is_valid_market(""),    "empty market invalid");
}

void test_exchange_validation() {
    CHECK(is_valid_exchange("SSE"),  "SSE is valid exchange");
    CHECK(is_valid_exchange("SZSE"), "SZSE is valid exchange");
    CHECK(!is_valid_exchange("NYSE"), "NYSE not supported");
    CHECK(!is_valid_exchange(""),    "empty exchange invalid");
}

void test_percentage_validation() {
    CHECK(is_valid_percentage(std::nullopt), "nullopt percentage is valid (missing)");
    CHECK(is_valid_percentage(0.0),   "0.0 is valid");
    CHECK(is_valid_percentage(0.035), "3.5% (0.035) is valid");
    CHECK(is_valid_percentage(0.5),   "50% (0.5) is valid");
    CHECK(is_valid_percentage(1.0),   "100% (1.0) is valid");
    CHECK(!is_valid_percentage(-0.01), "negative percentage invalid");
    CHECK(!is_valid_percentage(1.01),  ">100% percentage invalid");
}

void test_date_format_validation() {
    CHECK(is_valid_date_format("2026-07-11"), "valid date YYYY-MM-DD");
    CHECK(!is_valid_date_format("2026/07/11"), "wrong separators");
    CHECK(!is_valid_date_format("2026-7-11"),  "single digit month");
    CHECK(!is_valid_date_format("26-07-11"),   "2-digit year");
    CHECK(!is_valid_date_format(""),           "empty date");
    CHECK(!is_valid_date_format("abcdefghij"), "non-digit chars");
}

void test_fund_creation() {
    Fund f;
    f.id = 1;
    f.name = "沪深300ETF";
    f.manager = "华夏基金";
    f.fund_type = "股票型";
    f.listing_status = "上市";

    CHECK_EQ(f.id, 1, "fund id");
    CHECK_EQ(f.name, "沪深300ETF", "fund name");
    CHECK_EQ(f.manager, "华夏基金", "fund manager");
    CHECK_EQ(f.fund_type, "股票型", "fund type");
    CHECK_EQ(f.listing_status, "上市", "listing status");
}

void test_fund_listing() {
    FundListing fl;
    fl.fund_id = 1;
    fl.exchange = Exchange::SSE;
    fl.symbol = "510300";
    fl.currency = "CNY";

    CHECK_EQ(fl.fund_id, 1, "fund listing id");
    CHECK_EQ(fl.exchange, Exchange::SSE, "exchange SSE");
    CHECK_EQ(fl.symbol, "510300", "symbol");
    CHECK_EQ(fl.currency, "CNY", "currency default CNY");
}

void test_benchmark_index() {
    BenchmarkIndex bi;
    bi.code = "000300";
    bi.name = "沪深300";
    bi.provider = "CSI";

    CHECK_EQ(bi.code, "000300", "benchmark code");
    CHECK_EQ(bi.name, "沪深300", "benchmark name");
}

void test_index_valuation_with_optional() {
    IndexValuation iv;
    iv.benchmark_code = "000300";
    iv.valuation_date = "2026-07-11";
    iv.pe_ttm = 12.5;
    iv.pb = 1.5;
    iv.dividend_yield = 0.035;  // 3.5%
    iv.source = "mock";
    iv.collected_at = "2026-07-11T10:00:00Z";

    CHECK_EQ(iv.benchmark_code, "000300", "valuation benchmark code");
    CHECK(iv.pe_ttm.has_value(), "pe_ttm has value");
    CHECK_EQ(iv.pe_ttm.value(), 12.5, "pe_ttm value");
    CHECK_EQ(iv.pb.value(), 1.5, "pb value");
    CHECK_EQ(iv.dividend_yield.value(), 0.035, "dividend yield stored as decimal");

    // Test missing values
    IndexValuation empty;
    CHECK(!empty.pe_ttm.has_value(), "default pe_ttm is nullopt");
    CHECK(!empty.pb.has_value(), "default pb is nullopt");
    CHECK(!empty.dividend_yield.has_value(), "default dividend_yield is nullopt");
}

void test_fund_distribution() {
    FundDistribution fd;
    fd.symbol = "510300";
    fd.amount_per_share = 0.50;
    fd.ex_date = "2026-06-01";
    fd.record_date = "2026-06-02";
    fd.pay_date = "2026-06-10";
    fd.currency = "CNY";
    fd.source = "mock";

    CHECK_EQ(fd.symbol, "510300", "distribution symbol");
    CHECK_EQ(fd.amount_per_share.value(), 0.50, "dividend amount");
    CHECK_EQ(fd.ex_date, "2026-06-01", "ex-dividend date");
    CHECK_EQ(fd.currency, "CNY", "distribution currency");
}

void test_exchange_to_string() {
    CHECK_EQ(std::string(to_string(Exchange::SSE)),  "SSE",  "SSE to string");
    CHECK_EQ(std::string(to_string(Exchange::SZSE)), "SZSE", "SZSE to string");
}

void test_market_to_string() {
    CHECK_EQ(std::string(to_string(Market::CN)), "CN", "CN to string");
}

void test_exchange_from_string() {
    auto sse = exchange_from_string("SSE");
    CHECK(sse.has_value(), "SSE from string");
    CHECK_EQ(sse.value(), Exchange::SSE, "SSE parsed correctly");

    auto invalid = exchange_from_string("INVALID");
    CHECK(!invalid.has_value(), "invalid exchange returns nullopt");
}

void test_market_from_string() {
    auto cn = market_from_string("CN");
    CHECK(cn.has_value(), "CN from string");
    CHECK_EQ(cn.value(), Market::CN, "CN parsed correctly");

    auto invalid = market_from_string("HK");
    CHECK(!invalid.has_value(), "HK market returns nullopt");
}

void test_collection_result() {
    CollectionResult cr;
    cr.success = true;
    cr.symbol = "510300";
    cr.items_collected = 5;
    cr.collected_at = "2026-07-11T10:00:00Z";

    CHECK(cr.success, "collection success");
    CHECK_EQ(cr.symbol, "510300", "collection symbol");
    CHECK_EQ(cr.items_collected, 5, "items collected count");
}

} // namespace

int main() {
    test_etf_code_validation();
    test_market_validation();
    test_exchange_validation();
    test_percentage_validation();
    test_date_format_validation();
    test_fund_creation();
    test_fund_listing();
    test_benchmark_index();
    test_index_valuation_with_optional();
    test_fund_distribution();
    test_exchange_to_string();
    test_market_to_string();
    test_exchange_from_string();
    test_market_from_string();
    test_collection_result();

    std::cout << "\n=== " << tests << " tests, " << failures << " failures ===" << std::endl;
    return failures > 0 ? 1 : 0;
}