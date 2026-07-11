#ifndef INDEX_FUND_MONITOR_MODELS_MODELS_HPP
#define INDEX_FUND_MONITOR_MODELS_MODELS_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

enum class Market {
    CN,
};

enum class Exchange {
    SSE,
    SZSE,
};

struct Fund {
    int64_t id = 0;
    std::string name;
    std::string manager;
    std::string fund_type;
    std::string listing_status;
};

struct FundListing {
    int64_t fund_id = 0;
    Exchange exchange = Exchange::SSE;
    std::string symbol;
    std::string currency = "CNY";
};

struct BenchmarkIndex {
    std::string code;
    std::string name;
    std::string provider;
};

struct FundBenchmark {
    int64_t id = 0;
    std::string fund_symbol;
    std::string benchmark_code;
    std::string benchmark_name;
    std::string provider;
    std::string effective_date;
    std::string source;
};

struct IndexValuation {
    std::string benchmark_code;
    std::string valuation_date;
    std::optional<double> pe_ttm;
    std::optional<double> pb;
    std::optional<double> dividend_yield;
    std::string source;
    std::string collected_at;
};

struct FundDistribution {
    std::string symbol;
    std::optional<double> amount_per_share;
    std::string ex_date;
    std::string record_date;
    std::string pay_date;
    std::string currency = "CNY";
    std::string source;
};

struct DailyPrice {
    std::string symbol;
    std::string trade_date;
    std::optional<double> open_price;
    std::optional<double> close_price;
    std::optional<double> high_price;
    std::optional<double> low_price;
    std::optional<double> volume;
    std::optional<double> amount;
    std::string source;
    std::string collected_at;
};

struct DataSource {
    std::string name;
    std::string url;
    std::string description;
};

struct CollectionResult {
    bool success = false;
    std::string symbol;
    std::string error_message;
    int items_collected = 0;
    std::string collected_at;
};

// Utility functions

std::string_view to_string(Market m);
std::string_view to_string(Exchange e);

std::optional<Market> market_from_string(std::string_view s);
std::optional<Exchange> exchange_from_string(std::string_view s);

#endif