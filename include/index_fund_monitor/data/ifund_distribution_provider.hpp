#ifndef INDEX_FUND_MONITOR_DATA_IFUND_DISTRIBUTION_PROVIDER_HPP
#define INDEX_FUND_MONITOR_DATA_IFUND_DISTRIBUTION_PROVIDER_HPP

#include <string>
#include <vector>

#include "index_fund_monitor/data/data_result.hpp"
#include "index_fund_monitor/models/models.hpp"

struct FundDistributionResult {
    bool success = false;
    ProviderError error;
    std::vector<FundDistribution> distributions;
};

class IFundDistributionProvider {
public:
    virtual ~IFundDistributionProvider() = default;
    virtual FundDistributionResult fetch(const std::string& symbol) = 0;
};

#endif