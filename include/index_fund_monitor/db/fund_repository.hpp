#ifndef INDEX_FUND_MONITOR_DB_FUND_REPOSITORY_HPP
#define INDEX_FUND_MONITOR_DB_FUND_REPOSITORY_HPP

#include <optional>
#include <string>
#include <vector>

#include "index_fund_monitor/models/models.hpp"

class Database;

struct FundRecord {
    int64_t id = 0;
    std::string symbol;
    Exchange exchange = Exchange::SSE;
    std::string name;
    std::string currency = "CNY";
    std::string manager;
    std::string fund_type;
    std::string listing_status;
    std::string created_at;
};

class FundRepository {
public:
    explicit FundRepository(Database& db);

    bool add(const std::string& symbol, Exchange exchange, const std::string& name);
    bool remove(const std::string& symbol, Exchange exchange);
    std::vector<FundRecord> list() const;
    std::optional<FundRecord> find(const std::string& symbol, Exchange exchange) const;
    bool exists(const std::string& symbol, Exchange exchange) const;

private:
    Database& db_;
};

std::optional<Exchange> deduce_exchange(std::string_view symbol);

#endif