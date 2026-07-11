#ifndef INDEX_FUND_MONITOR_DATA_IFUND_DIRECTORY_PROVIDER_HPP
#define INDEX_FUND_MONITOR_DATA_IFUND_DIRECTORY_PROVIDER_HPP

#include <optional>
#include <vector>

#include "index_fund_monitor/data/data_result.hpp"
#include "index_fund_monitor/db/fund_repository.hpp"

struct FundDirectoryResult {
    bool success = false;
    ProviderError error;
    std::vector<FundRecord> funds;
};

class IFundDirectoryProvider {
public:
    virtual ~IFundDirectoryProvider() = default;
    virtual FundDirectoryResult fetch_all() = 0;
};

#endif