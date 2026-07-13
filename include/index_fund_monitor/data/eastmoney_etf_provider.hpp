#ifndef INDEX_FUND_MONITOR_DATA_EASTMONEY_ETF_PROVIDER_HPP
#define INDEX_FUND_MONITOR_DATA_EASTMONEY_ETF_PROVIDER_HPP

#include <memory>
#include <string>
#include <vector>

#include "index_fund_monitor/data/data_result.hpp"
#include "index_fund_monitor/db/fund_repository.hpp"

class IHttpClient;

struct ETFDirectoryResult {
    bool success = false;
    ProviderError error;
    std::vector<FundRecord> funds;
    std::string data_date;
    std::string collected_at;
};

class EastMoneyETFProvider {
public:
    EastMoneyETFProvider(std::shared_ptr<IHttpClient> http_client);
    ETFDirectoryResult fetch_all();

    void set_fixture_dir(const std::string& dir);
    void set_simulate_network_error(bool v);
    void set_simulate_format_error(bool v);

    // For testing without network
    ETFDirectoryResult parse_response(const std::string& json_body);

private:
    std::shared_ptr<IHttpClient> http_client_;
    std::string fixture_dir_;
    bool simulate_network_error_ = false;
    bool simulate_format_error_ = false;
    bool use_fixture_ = false;
};

#endif