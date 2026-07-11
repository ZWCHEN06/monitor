#ifndef INDEX_FUND_MONITOR_DATA_IPRICE_PROVIDER_HPP
#define INDEX_FUND_MONITOR_DATA_IPRICE_PROVIDER_HPP

#include <string>
#include <vector>

#include "index_fund_monitor/data/data_result.hpp"
#include "index_fund_monitor/models/models.hpp"

struct PriceResult {
    bool success = false;
    ProviderError error;
    std::vector<DailyPrice> prices;
};

class IPriceProvider {
public:
    virtual ~IPriceProvider() = default;
    virtual PriceResult fetch(const std::string& symbol) = 0;
};

#endif